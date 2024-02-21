#include "NdkVideoDecoder.h"

#include <media/NdkMediaFormat.h>
#include <media/NdkMediaCodec.h>

#include <AndroidClient/VideoDecoderBackend.h>
#include <Platform/Core/EnumClassBitwiseOperators.h>
#include <Platform/Vulkan/Texture.h>
#include <Platform/Vulkan/EffectPass.h>
#include <Platform/Vulkan/RenderPlatform.h>
#include <TeleportCore/ErrorHandling.h>
#include <fmt/core.h>
#include <magic_enum/magic_enum.hpp>

#include <iostream>
#include <android/hardware_buffer_jni.h>
#include <android/native_window.h>
#include "ThisPlatform/Threads.h"

using namespace teleport;
using namespace android;

template<typename _Ty>
void BitwiseSet(std::atomic<_Ty>& status, const _Ty& value)
{
	status = status.load() | value;
}
template<typename _Ty>
void BitwiseUnset(std::atomic<_Ty>& status, const _Ty& value)
{
	status = status.load() & ~(value);
}
template<typename _Ty>
void BitwiseDecrement(std::atomic<_Ty>& status, const _Ty& value)
{
	if ((status.load() & value) != static_cast<_Ty>(0))
	{
		uint32_t decrementValue = static_cast<uint32_t>(value) / 0xF;
		status = static_cast<_Ty>(static_cast<uint32_t>(status.load()) - decrementValue);
	}
}

#if NDK_VIDEO_DECODER_LOG
#define NDK_VIDEO_DECODER_LOG_MSG_COUT(msg) { TELEPORT_COUT << "NdkVideoDecoder - " << msg << std::endl; }
#define NDK_VIDEO_DECODER_LOG_FMT_COUT(msg, ...) NDK_VIDEO_DECODER_LOG_MSG_COUT(fmt::format(msg, __VA_ARGS__))

#define NDK_VIDEO_DECODER_LOG_MSG_CERR(msg) { TELEPORT_CERR << "NdkVideoDecoder - " << msg << std::endl; }
#define NDK_VIDEO_DECODER_LOG_FMT_CERR(msg, ...) NDK_VIDEO_DECODER_LOG_MSG_CERR(fmt::format(msg, __VA_ARGS__))
#else
#define NDK_VIDEO_DECODER_LOG_MSG_COUT(msg)
#define NDK_VIDEO_DECODER_LOG_FMT_COUT(msg, ...)

#define NDK_VIDEO_DECODER_LOG_MSG_CERR(msg)
#define NDK_VIDEO_DECODER_LOG_FMT_CERR(msg, ...)
#endif
#define AMEDIA_CHECK(r)\
{\
	media_status_t res = r;\
	if(res != AMEDIA_OK)\
	{\
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Failed AMEDIA_CHECK.");\
		return;\
	}\
}

#define VK_CHECK(r)\
{\
	vk::Result res = r;\
	if (res != vk::Result::eSuccess)\
	{\
		SIMUL_VK_CHECK(res);\
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Failed VK_CHECK.");\
		return; \
	}\
}

static bool memory_type_from_properties(vk::PhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t* typeIndex)
{
	// Search memtypes to find first index with those properties
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if ((typeBits & 1) == 1)
		{
			// Type is available, does it match user properties?
			if ((memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask)
			{
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}
	// No memory types matched, return failure
	return false;
}
static int VulkanFormatToHardwareBufferFormat(VkFormat v)
{
	switch (v)
	{
	case VK_FORMAT_R8G8B8A8_UNORM:
		return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
	case VK_FORMAT_R8G8B8_UNORM:
		return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
	case VK_FORMAT_R5G6B5_UNORM_PACK16:
		return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
	case VK_FORMAT_D16_UNORM:
		return AHARDWAREBUFFER_FORMAT_D16_UNORM;
	case VK_FORMAT_X8_D24_UNORM_PACK32:
		return AHARDWAREBUFFER_FORMAT_D24_UNORM;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT;
	case VK_FORMAT_D32_SFLOAT:
		return AHARDWAREBUFFER_FORMAT_D32_FLOAT;
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT;
	case VK_FORMAT_S8_UINT:
		return AHARDWAREBUFFER_FORMAT_S8_UINT;
	default:
		TELEPORT_BREAK_ONCE("");
		return 0;
	}
}

NdkVideoDecoder::NdkVideoDecoder(VideoDecoderBackend* d, avs::VideoCodec codecType)
{
	this->videoDecoder = d;
	this->codecType = codecType;
}

NdkVideoDecoder::~NdkVideoDecoder()
{
	Shutdown();
}

void NdkVideoDecoder::Initialize(platform::crossplatform::RenderPlatform* p, platform::crossplatform::Texture* texture)
{
	//Initial checks
	if (!p || !texture)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Can not Initialize - RenderPlatform or Texture is nullptr.");
		return;
	}
	if (decoderConfigured)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("VideoDecoder: Cannot initialize: already configured");
		return;
	}

	//Class member assignments
	renderPlatform = (platform::vulkan::RenderPlatform*)p;
	targetTexture = (platform::vulkan::Texture*)texture;
	physicalDeviceMemoryProperties = renderPlatform->GetVulkanGPU()->getMemoryProperties(); 
	textureConversionEffect = p->CreateEffect("texture_conversion");

	//Create Decoder
	decoder = AMediaCodec_createDecoderByType(GetCodecMimeType());
	if (!decoder)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Failed to create Decoder.");
		return;
	}

	//Create ImageReader
	AMEDIA_CHECK(AImageReader_newWithUsage(targetTexture->width, targetTexture->length, AIMAGE_FORMAT_YUV_420_888, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, videoDecoderParams.maxDecodePictureBufferCount, &imageReader));
	
	//NativeWindow is managed by the ImageReader.
	ANativeWindow* nativeWindow = nullptr;
	AMEDIA_CHECK(AImageReader_getWindow(imageReader, &nativeWindow));
	AHardwareBuffer_Format nativeWindowFormat = (AHardwareBuffer_Format)ANativeWindow_getFormat(nativeWindow);
	
	//Set up Callbacks before configure
	AMediaCodecOnAsyncNotifyCallback callback;
	callback.onAsyncInputAvailable = onAsyncInputAvailable;
	callback.onAsyncOutputAvailable = onAsyncOutputAvailable;
	callback.onAsyncFormatChanged = onAsyncFormatChanged;
	callback.onAsyncError = onAsyncError;
	AMEDIA_CHECK(AMediaCodec_setAsyncNotifyCallback(decoder, callback, this));
	
	//Configure codec. Guessing the following is equivalent:
	//https://developer.android.com/reference/android/media/MediaFormat
	AMediaFormat* configFormat = AMediaFormat_new();
	AMediaFormat_setString	(configFormat, AMEDIAFORMAT_KEY_MIME,			GetCodecMimeType());
	AMediaFormat_setInt64	(configFormat, AMEDIAFORMAT_KEY_DURATION,		INT64_MAX);
	AMediaFormat_setInt32	(configFormat, AMEDIAFORMAT_KEY_WIDTH,			targetTexture->width);
	AMediaFormat_setInt32	(configFormat, AMEDIAFORMAT_KEY_HEIGHT,			targetTexture->length);
	AMediaFormat_setInt32	(configFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT,	(int32_t)CodecCapabilities::COLOR_FormatYUV420Flexible);
	AMediaFormat_setInt32	(configFormat, AMEDIAFORMAT_KEY_MAX_WIDTH,		targetTexture->width);
	AMediaFormat_setInt32	(configFormat, AMEDIAFORMAT_KEY_MAX_HEIGHT,		targetTexture->length);
	AMEDIA_CHECK(AMediaCodec_configure(decoder, configFormat, nativeWindow, nullptr, 0));
	decoderConfigured = true;

	//Start Decoder
	AMEDIA_CHECK(AMediaCodec_start(decoder));

	//Check config and output formats for codec
	auto GetAMediaFormatStringAndDelete = [](AMediaFormat* format) -> std::string
	{
		if (format)
		{	
			std::string result = AMediaFormat_toString(format);
			AMediaFormat_delete(format);
			return result;
		}
		else
		{
			return "";
		}
	};

	std::string decoderConfigFormatStr = GetAMediaFormatStringAndDelete(configFormat);
	std::string decoderInputFormatStr = GetAMediaFormatStringAndDelete(AMediaCodec_getInputFormat(decoder));
	std::string decoderOutputFormatStr = GetAMediaFormatStringAndDelete(AMediaCodec_getOutputFormat(decoder));

	//Check ImageReader format and maxImages
	AIMAGE_FORMATS imageReaderFormat = AIMAGE_FORMATS(0);
	AMEDIA_CHECK(AImageReader_getFormat(imageReader, (int32_t*)&imageReaderFormat));
	NDK_VIDEO_DECODER_LOG_FMT_CERR("ImageReader Format - {0}", magic_enum::enum_name<AIMAGE_FORMATS>(imageReaderFormat));
	int32_t maxImages = 0;
	AMEDIA_CHECK(AImageReader_getMaxImages(imageReader, &maxImages));
	NDK_VIDEO_DECODER_LOG_FMT_CERR("ImageReader MaxImages - {0}", maxImages);
	
	//Set up reflected textures
	reflectedTextures.resize(maxImages);
	for (int i = 0; i < maxImages; i++)
		reflectedTextures[i].sourceTexture = (platform::vulkan::Texture*)renderPlatform->CreateTexture(fmt::format("video source {0}", i).c_str());
	
	//Set up ImageListener and onImageAvailable callback
	static AImageReader_ImageListener imageListener;
	imageListener.context = this;
	imageListener.onImageAvailable = onAsyncImageAvailable;
	AMEDIA_CHECK(AImageReader_setImageListener(imageReader, &imageListener));

	videoDecoder->GetDecoderStatus() = avs::DecoderStatus::DecoderAvailable;
	
	//Start Async thread
	stopProcessBuffersThread = false;
	processBuffersThread = new std::thread(&NdkVideoDecoder::processBuffersOnThread, this);
	SetThreadName(*processBuffersThread,"NdkVideoDecoder::processBuffersOnThread");
}

void NdkVideoDecoder::Shutdown()
{
	stopProcessBuffersThread = true;
	if (processBuffersThread)
	{
		while (!processBuffersThread->joinable()) {}
		processBuffersThread->join();
		delete processBuffersThread;
		processBuffersThread = nullptr;
	}
	if (!decoderConfigured)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Cannot shutdown Decoder: Not configured");
		return;
	}

	AImageReader_delete(imageReader);
	imageReader = nullptr;

	AMediaCodec_flush(decoder);
	AMediaCodec_stop(decoder);
	AMediaCodec_delete(decoder);
	decoder = nullptr;
	decoderConfigured = false; 
	videoDecoder->GetDecoderStatus() = avs::DecoderStatus::DecoderUnavailable;

	displayRequests = 0;

	if (textureConversionEffect)
	{
		delete textureConversionEffect;
		textureConversionEffect = nullptr;
	}

}

bool NdkVideoDecoder::Decode(std::vector<uint8_t>& buffer, avs::VideoPayloadType payloadType, bool lastPayload)
{
	BitwiseSet(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::ReceivingVideoStream);
	
	BitwiseDecrement(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::QueuingVideoStreamBuffer);

	if (!decoderConfigured)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Cannot decode Buffer: Decoder not configured");
		return false;
	}

	//Set up start codes
	int payloadFlags = 0;
	switch (payloadType)
	{
	case avs::VideoPayloadType::VPS:
	case avs::VideoPayloadType::PPS:
	case avs::VideoPayloadType::SPS:
	case avs::VideoPayloadType::ALE:
		payloadFlags = AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;
		break;
	default:
		break;
	}
	std::vector<uint8_t> startCodes;
	switch (payloadFlags)
	{
	case AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG:
		startCodes = { 0, 0, 0, 1 };
		break;
	default:
		startCodes = { 0, 0, 1 };
		break;
	}

	if (!lastPayload)
	{
		// Signifies partial frame data. For all VCLs in a frame besides the last one. Needed for H264.
		if (payloadFlags == 0)
		{
			payloadFlags = AMEDIACODEC_BUFFER_FLAG_PARTIAL_FRAME;
		}
	}

	std::vector<uint8_t> inputBuffer = startCodes;
	inputBuffer.insert(inputBuffer.end(), buffer.begin(), buffer.end());
	
	int32_t bufferId = QueueInputBuffer(inputBuffer, payloadFlags, lastPayload);
	BitwiseSet(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::QueuingVideoStreamBuffer);
	if (lastPayload && bufferId >= 0)
	{
		displayRequests++;
		return true;
	}

	return false;
}

bool NdkVideoDecoder::Display()
{
	if (!decoderConfigured)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Cannot display output: Decoder not configured");
		return false;
	}
	while (displayRequests > 0)
	{
		if (ReleaseOutputBuffer(displayRequests == 1) > -2)
			displayRequests--;
	}
	return true;
}

void NdkVideoDecoder::CopyVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext)
{
	BitwiseDecrement(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::FrameAvailable);

	if (!decoderConfigured)
		return;
	
	std::unique_lock<std::mutex> lock(texture_mutex);
	// start freeing images after 12 frames.
	for (size_t i = 0; i < texturesToFree.size(); i++)
		FreeTexture(texturesToFree[i]);
	texturesToFree.clear();

	if (!renderPlatform)
		return;
	if (nextImageIndex < 0)
		return;

	bool bt601 = ycbcrModel == vk::SamplerYcbcrModelConversion::eYcbcr601;
	bool bt709 = ycbcrModel == vk::SamplerYcbcrModelConversion::eYcbcr709;
	bool bt2020 = ycbcrModel == vk::SamplerYcbcrModelConversion::eYcbcr2020;

	if (!bt601 && !bt709 && !bt2020)
		return;

	ReflectedTexture& reflectedTexture = reflectedTextures[nextImageIndex];
	if (!reflectedTexture.nextImage)
		return;

	nextImageIndex = -1;
	platform::crossplatform::TextureCreate textureCreate;
	textureCreate.w = targetTexture->width;
	textureCreate.l = targetTexture->length;
	textureCreate.d = 1;
	textureCreate.arraysize = 1;
	textureCreate.mips = 1;
	textureCreate.f = platform::crossplatform::PixelFormat::UNDEFINED;//targetTexture->pixelFormat;
	textureCreate.numOfSamples = 1;
	textureCreate.make_rt = false;
	textureCreate.setDepthStencil = false;
//	textureCreate.need_srv = true;
	textureCreate.cubemap = false;
	textureCreate.external_texture = (void*)reflectedTexture.videoSourceVkImage;
	textureCreate.forceInit = true;

	reflectedTexture.sourceTexture->InitFromExternalTexture(renderPlatform, &textureCreate);
	if (!reflectedTexture.sourceTexture->IsValid())
		return;
	reflectedTexture.sourceTexture->AssumeLayout(vk::ImageLayout::eUndefined);

	// Can't use RenderPlatform::CopyTexture because we can't use transfer_src.
	auto* effect = textureConversionEffect;
	int passIndex = bt2020 ? 2 : bt709 ? 1 : bt601 ? 0 : 1;
	auto* effectPass = (platform::vulkan::EffectPass*)effect->GetTechniqueByName("ycbcr_to_rgb")->GetPass(passIndex);
	effectPass->SetVideoSource(true);
	auto srcResource = effect->GetShaderResource("ycbcrTexture");
	auto dstResource = effect->GetShaderResource("rgbTexture");
	renderPlatform->SetTexture(deviceContext, srcResource, reflectedTexture.sourceTexture);
	renderPlatform->SetUnorderedAccessView(deviceContext, dstResource, targetTexture);
	renderPlatform->ApplyPass(deviceContext, effectPass);
	int w = (targetTexture->width + 7) / 8;
	int l = (targetTexture->length + 7) / 8;
	renderPlatform->DispatchCompute(deviceContext, w, l, 1);
	renderPlatform->UnapplyPass(deviceContext);

	BitwiseSet(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::FrameAvailable);
}

//Callbacks
void NdkVideoDecoder::onAsyncInputAvailable(AMediaCodec* codec, void* userdata, int32_t inputBufferId) //NDK MediaCodec_ thread
{
	NdkVideoDecoder* ndkVideoDecoder = (NdkVideoDecoder*)userdata;
	if (codec != ndkVideoDecoder->decoder)
		return;

	NDK_VIDEO_DECODER_LOG_FMT_COUT("onAsyncInputAvailable(): New Input Buffer: {0}", inputBufferId);
	ndkVideoDecoder->nextInputBuffers.push_back({ inputBufferId, 0, -1 });
}

void NdkVideoDecoder::onAsyncOutputAvailable(AMediaCodec* codec, void* userdata, int32_t outputBufferId, AMediaCodecBufferInfo* bufferInfo) //NDK MediaCodec_ thread
{
	NdkVideoDecoder* ndkVideoDecoder = (NdkVideoDecoder*)userdata;
	if (codec != ndkVideoDecoder->decoder)
		return;

	NDK_VIDEO_DECODER_LOG_FMT_COUT("onAsyncOutputAvailable(): New Output Buffer: {0}", outputBufferId);
	ndkVideoDecoder->outputBuffers.push_back({ outputBufferId, bufferInfo->offset, bufferInfo->size, bufferInfo->presentationTimeUs, bufferInfo->flags });
}

void NdkVideoDecoder::onAsyncFormatChanged(AMediaCodec* codec, void* userdata, AMediaFormat* format) //NDK MediaCodec_ thread
{
	NdkVideoDecoder* ndkVideoDecoder = (NdkVideoDecoder*)userdata;

	CodecCapabilities formatColour = CodecCapabilities(0);
	AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, (int32_t*)&formatColour);
	NDK_VIDEO_DECODER_LOG_FMT_COUT("onAsyncFormatChanged(): CodecCapabilities: {0}", formatColour);
}

void NdkVideoDecoder::onAsyncError(AMediaCodec* codec, void* userdata, media_status_t error, int32_t actionCode, const char* detail) //NDK MediaCodec_ thread
{
	NdkVideoDecoder* ndkVideoDecoder = (NdkVideoDecoder*)userdata;
	NDK_VIDEO_DECODER_LOG_FMT_COUT("onAsyncError(): {0}", detail);
}

void NdkVideoDecoder::onAsyncImageAvailable(void* context, AImageReader* reader)  //ImageReader thread
{
	NdkVideoDecoder* ndkVideoDecoder = (NdkVideoDecoder*)context;
	BitwiseDecrement(ndkVideoDecoder->videoDecoder->GetDecoderStatus(), avs::DecoderStatus::DecodingVideoStream);
	BitwiseDecrement(ndkVideoDecoder->videoDecoder->GetDecoderStatus(), avs::DecoderStatus::ProcessingOutputFrameFromDecoder);

	ndkVideoDecoder->reflectedTextureIndex = (ndkVideoDecoder->reflectedTextureIndex + 1) % ndkVideoDecoder->reflectedTextures.size();
	NDK_VIDEO_DECODER_LOG_FMT_COUT("onAsyncImageAvailable(): {0}", ndkVideoDecoder->reflectedTextureIndex);
	ReflectedTexture& reflectedTexture = ndkVideoDecoder->reflectedTextures[ndkVideoDecoder->reflectedTextureIndex];

	//AIMAGE_FORMATS format1 = AIMAGE_FORMATS(0);
	//AMEDIA_CHECK(AImageReader_getFormat(ndkVideoDecoder->imageReader, (int32_t*)&format1));
	//int32_t maxImages = 0;
	//AMEDIA_CHECK(AImageReader_getMaxImages(ndkVideoDecoder->imageReader, &maxImages));

	auto res = AImageReader_acquireLatestImage(ndkVideoDecoder->imageReader, &reflectedTexture.nextImage);
	if (res != AMEDIA_OK)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("AImageReader_acquireLatestImage() failed.");
		ndkVideoDecoder->FreeTexture(ndkVideoDecoder->reflectedTextureIndex);
		ndkVideoDecoder->reflectedTextureIndex--;
		return;
	}
	else
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("AImageReader_acquireLatestImage() succeeded.");
	}

	BitwiseSet(ndkVideoDecoder->videoDecoder->GetDecoderStatus(), avs::DecoderStatus::DecodingVideoStream);
	BitwiseSet(ndkVideoDecoder->videoDecoder->GetDecoderStatus(), avs::DecoderStatus::ProcessingOutputFrameFromDecoder);

	/*int32_t numPlanes = 0;
	AImage_getNumberOfPlanes(reflectedTexture.nextImage, &numPlanes);
	std::vector<int32_t> rowStrides, pixelStrides;
	rowStrides.reserve(numPlanes);
	pixelStrides.reserve(numPlanes);
	for (int32_t i = 0; i < numPlanes; i++)
	{
		int32_t rowStride;
		AImage_getPlaneRowStride(reflectedTexture.nextImage, i, &rowStride);
		rowStrides.push_back(rowStride);
		int32_t pixelStride;
		AImage_getPlanePixelStride(reflectedTexture.nextImage, i, &pixelStride);
		pixelStrides.push_back(pixelStride);
	}

	AIMAGE_FORMATS format2;
	AImage_getFormat(reflectedTexture.nextImage, (int32_t*)&format2);*/
	
	std::unique_lock<std::mutex> lock(ndkVideoDecoder->texture_mutex);
	// start freeing images after 12 frames.
	size_t idx = (ndkVideoDecoder->reflectedTextureIndex + 4) % ndkVideoDecoder->reflectedTextures.size();
	ndkVideoDecoder->texturesToFree.push_back(idx);

	//Get AHardwareBuffer and AHardwareBuffer_Desc
	AHardwareBuffer* hardwareBuffer = nullptr;
	AMEDIA_CHECK(AImage_getHardwareBuffer(reflectedTexture.nextImage, &hardwareBuffer));
	AHardwareBuffer_Desc hardwareBufferDesc;
	AHardwareBuffer_describe(hardwareBuffer, &hardwareBufferDesc);
	const AHardwareBuffer_Format& hardwareBufferFormat = AHardwareBuffer_Format(hardwareBufferDesc.format);
	const AHardwareBuffer_UsageFlags& hardwareBufferUsage = AHardwareBuffer_UsageFlags(hardwareBufferDesc.usage);

	//Vulkan Properties and FormatProperties from the AHardwareBuffer
	vk::Device* vulkanDevice = ndkVideoDecoder->renderPlatform->AsVulkanDevice();
	vk::AndroidHardwareBufferPropertiesANDROID hardwareBufferProperties;
	vk::AndroidHardwareBufferFormatPropertiesANDROID hardwareBufferFormatProperties;
	hardwareBufferProperties.pNext = &hardwareBufferFormatProperties;
	VK_CHECK(vulkanDevice->getAndroidHardwareBufferPropertiesANDROID(hardwareBuffer, &hardwareBufferProperties));

	//Set the YCbCr Model for later conversion.
	ndkVideoDecoder->ycbcrModel = hardwareBufferFormatProperties.suggestedYcbcrModel;
	ndkVideoDecoder->externalFormat = hardwareBufferFormatProperties.externalFormat;

	//Set ExternalFormatANDROID
	vk::ExternalFormatANDROID externalFormatAndroid;
	externalFormatAndroid.pNext = nullptr;
	externalFormatAndroid.externalFormat = hardwareBufferFormatProperties.externalFormat;

	//Set ExternalMemoryImageCreateInfo
	vk::ExternalMemoryImageCreateInfo externalMemoryImageCI;
	externalMemoryImageCI.pNext = &externalFormatAndroid;
	externalMemoryImageCI.handleTypes = vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;

	//Set ImageCreateInfo and create the Image
	vk::ImageCreateInfo imageCI = vk::ImageCreateInfo();
	imageCI.pNext = &externalMemoryImageCI;
	imageCI.flags = vk::ImageCreateFlagBits(0);
	imageCI.imageType = vk::ImageType::e2D;
	imageCI.format = vk::Format::eUndefined; //Set by vk::ExternalFormatANDROID
	imageCI.extent = vk::Extent3D((uint32_t)ndkVideoDecoder->targetTexture->width, (uint32_t)ndkVideoDecoder->targetTexture->length, 1);
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = hardwareBufferDesc.layers;
	imageCI.samples = vk::SampleCountFlagBits::e1;
	imageCI.tiling = vk::ImageTiling::eOptimal;
	imageCI.usage = vk::ImageUsageFlagBits::eSampled;
	imageCI.sharingMode = vk::SharingMode::eExclusive;
	imageCI.queueFamilyIndexCount = 0;
	imageCI.pQueueFamilyIndices = nullptr;
	imageCI.initialLayout = vk::ImageLayout::eUndefined;
	VK_CHECK(vulkanDevice->createImage(&imageCI, nullptr, &reflectedTexture.videoSourceVkImage));
	platform::vulkan::SetVulkanName(ndkVideoDecoder->renderPlatform, reflectedTexture.videoSourceVkImage, fmt::format("NdkVideoDecoder: VideoSource {}: VkImage", ndkVideoDecoder->reflectedTextureIndex));
	

	//Set up MemoryDedicatedAllocateInfo and ImportAndroidHardwareBufferInfoANDROID - Required for AHB images.
	vk::MemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo;
	memoryDedicatedAllocateInfo.image = reflectedTexture.videoSourceVkImage;
	
	vk::ImportAndroidHardwareBufferInfoANDROID importAndroidHardwareBufferInfo;
	importAndroidHardwareBufferInfo.pNext = &memoryDedicatedAllocateInfo;
	importAndroidHardwareBufferInfo.buffer = hardwareBuffer;

	//Set up MemoryAllocateInfo
	vk::MemoryAllocateInfo mem_alloc_info;
	mem_alloc_info.pNext = &importAndroidHardwareBufferInfo;
	mem_alloc_info.allocationSize = hardwareBufferProperties.allocationSize;
	memory_type_from_properties(ndkVideoDecoder->physicalDeviceMemoryProperties, hardwareBufferProperties.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal, &mem_alloc_info.memoryTypeIndex);
	
	//Allocate the memory within Vulkan.
	VK_CHECK(vulkanDevice->allocateMemory(&mem_alloc_info, nullptr, &reflectedTexture.videoSourceVkDeviceMemory));
	platform::vulkan::SetVulkanName(ndkVideoDecoder->renderPlatform, reflectedTexture.videoSourceVkDeviceMemory, fmt::format("NdkVideoDecoder: VideoSource {}: VkDeviceMemory", ndkVideoDecoder->reflectedTextureIndex));
	
	//Bind the back memory to the Image
	//Dedicated memory bindings require offset 0.
	vulkanDevice->bindImageMemory(reflectedTexture.videoSourceVkImage, reflectedTexture.videoSourceVkDeviceMemory, 0);

	NDK_VIDEO_DECODER_LOG_FMT_CERR("Converted AImage to VkImage. Index: {}.", ndkVideoDecoder->reflectedTextureIndex);
	ndkVideoDecoder->nextImageIndex = ndkVideoDecoder->reflectedTextureIndex;
}

//Others
void NdkVideoDecoder::FreeTexture(int index)
{
	if (index < 0 || index >= reflectedTextures.size())
		return;
	ReflectedTexture& reflectedTexture = reflectedTextures[index];
	if (reflectedTexture.nextImage)
	{
		renderPlatform->PushToReleaseManager(reflectedTexture.videoSourceVkDeviceMemory);
		AImage_delete(reflectedTexture.nextImage);
		reflectedTexture.nextImage = nullptr;
	}
}

int32_t NdkVideoDecoder::QueueInputBuffer(std::vector<uint8_t>& bytes, int flags, bool send)
{
	buffers_mutex.lock();
	dataBuffers.push_back({ bytes, flags, send });
	buffers_mutex.unlock();
	return 0;
}

int NdkVideoDecoder::ReleaseOutputBuffer(bool render)
{
	buffers_mutex.lock();
	if (!outputBuffers.size())
	{
		buffers_mutex.unlock();
		return 0;
	}
	buffers_mutex.unlock();
	return 0;
}

const char* NdkVideoDecoder::GetCodecMimeType()
{
	switch (codecType)
	{
	case avs::VideoCodec::H264:
		return "video/avc";
	case avs::VideoCodec::HEVC:
		return "video/hevc";
	default:
		return "Invalid";
	};
}

//Processing thread
void NdkVideoDecoder::processBuffersOnThread()
{
	SetThisThreadName("NdkVideoDecoder::processBuffersOnThread");
	while (!stopProcessBuffersThread)
	{
		buffers_mutex.lock();
		processInputBuffers();
		processOutputBuffers();
		processImages();
		buffers_mutex.unlock();
		//std::this_thread::sleep_for(std::chrono::nanoseconds(10000));
	}
}

void NdkVideoDecoder::processInputBuffers()
{
	BitwiseDecrement(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::AccumulatingVideoStreamBuffers);
	BitwiseDecrement(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::PassingVideoStreamToDecoder);

	// Returns the index of an input buffer to be filled with valid data or -1 if no such buffer is currently available.
	//ssize_t inputBufferID=AMediaCodec_dequeueInputBuffer(codec,0);
	//val inputBufferID = mDecoder.dequeueInputBuffer(0) // microseconds
	if (!dataBuffers.size())
	{
		//NDK_VIDEO_DECODER_LOG_MSG_CERR("Nothing to process, stalling.");
		return;
	}
	if (!nextInputBuffers.size())
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("Out of Buffers, queueing.");
		return;
	}
	BitwiseSet(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::AccumulatingVideoStreamBuffers);

	DataBuffer& dataBuffer = dataBuffers.front();
	bool send = dataBuffer.send;
	bool lastpacket = dataBuffer.send;
	send = false;
	InputBuffer& inputBuffer = nextInputBuffers[nextInputBufferIndex];

	size_t buffer_size = 0;
	size_t copiedSize = 0;
	uint8_t* targetBufferData = AMediaCodec_getInputBuffer(decoder, inputBuffer.inputBufferId, &buffer_size);
	if (!targetBufferData)
	{
		NDK_VIDEO_DECODER_LOG_MSG_COUT("AMediaCodec_getInputBuffer() failed.");
		return;
	}
	if (!inputBuffer.offset)
	{
		NDK_VIDEO_DECODER_LOG_FMT_COUT("AMediaCodec_getInputBuffer() {} got a size of {}.", inputBuffer.inputBufferId, buffer_size);
	}
	copiedSize = std::min(buffer_size - inputBuffer.offset, dataBuffer.bytes.size());

	bool add_to_new = !targetBufferData || copiedSize < dataBuffer.bytes.size();
	// if buffer is valid, copy our data into it.
	if (targetBufferData && copiedSize == dataBuffer.bytes.size())
	{
		// if flags changes and data is already on the buffer, send this buffer and add to a new one.
		if (inputBuffer.flags != -1 && dataBuffer.flags != inputBuffer.flags)
		{
			send = true;
			add_to_new = true;
		}
		else
		{
			memcpy(targetBufferData + inputBuffer.offset, dataBuffer.bytes.data(), copiedSize);
			inputBuffer.offset += copiedSize;
			inputBuffer.flags = dataBuffer.flags;
			//NDK_VIDEO_DECODER_LOG_FMT_COUT("Buffer {} at offset {} added: {} bytes with flag {}.", inputBuffer.inputBufferId, inputBuffer.offset, copiedSize, dataBuffer.flags);
			// over half the buffer filled then send
			if (inputBuffer.offset >= buffer_size / 2)
				send = true;
		}
	}
	else
	{
		NDK_VIDEO_DECODER_LOG_FMT_COUT("Buffer {} full. Sending.", inputBuffer.inputBufferId);
		send = true;
	}
	// offset now equals the accumulated size of the buffer.
	if (send && inputBuffer.offset > 0)
	{
		media_status_t res = AMediaCodec_queueInputBuffer(decoder, inputBuffer.inputBufferId, 0, inputBuffer.offset, 0, inputBuffer.flags);
		if (res != AMEDIA_OK)
		{
			NDK_VIDEO_DECODER_LOG_MSG_COUT("AMediaCodec_queueInputBuffer() failed.");
			return;
		}
		BitwiseSet(videoDecoder->GetDecoderStatus(), avs::DecoderStatus::PassingVideoStreamToDecoder);
		//if(lastpacket)
		//	__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","Last Packet.");
		NDK_VIDEO_DECODER_LOG_FMT_COUT("Buffer: {} SENT {} bytes with flag {}.", inputBuffer.inputBufferId, inputBuffer.offset, inputBuffer.flags);
		nextInputBuffers.erase(nextInputBuffers.begin() + nextInputBufferIndex);

		if (nextInputBufferIndex >= nextInputBuffers.size())
			nextInputBufferIndex = 0;
	}
	if (add_to_new)
	{
		InputBuffer& nextInputBuffer = nextInputBuffers[nextInputBufferIndex];
		uint8_t* targetBufferData2 = AMediaCodec_getInputBuffer(decoder, nextInputBuffer.inputBufferId, &buffer_size);
		if (targetBufferData2)
		{
			NDK_VIDEO_DECODER_LOG_FMT_COUT("A new Buffer {} added: {} bytes with flag {}.", inputBuffer.inputBufferId, dataBuffer.bytes.size(), dataBuffer.flags);
			memcpy(targetBufferData2, dataBuffer.bytes.data(), dataBuffer.bytes.size());
			nextInputBuffer.offset += dataBuffer.bytes.size();
			nextInputBuffer.flags = dataBuffer.flags;
		}
		else
		{
			NDK_VIDEO_DECODER_LOG_MSG_COUT("AMediaCodec_getInputBuffer() failed.");
			return;
		}
	}
	dataBuffers.pop_front();
}

void NdkVideoDecoder::processOutputBuffers()
{
	if (!outputBuffers.size())
		return;

	OutputBuffer& outputBuffer = outputBuffers.front();
	if ((outputBuffer.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0)
	{
		NDK_VIDEO_DECODER_LOG_MSG_COUT("Decoder: Codec config Buffer.");
		AMediaCodec_releaseOutputBuffer(decoder, outputBuffer.outputBufferId, false);
		return;
	}
	//auto bufferFormat = AMediaCodec_getOutputFormat(codec,outputBufferId); // option A
	// bufferFormat is equivalent to mOutputFormat
	// outputBuffer is ready to be processed or rendered.
	NDK_VIDEO_DECODER_LOG_FMT_COUT("Output available {}, size: {}.", outputBuffer.outputBufferId, outputBuffer.size);

	bool render = outputBuffer.size != 0;
	if (AMediaCodec_releaseOutputBuffer(decoder, outputBuffer.outputBufferId, true) != AMEDIA_OK)
	{
		NDK_VIDEO_DECODER_LOG_MSG_CERR("AMediaCodec_releaseOutputBuffer() failed.");
	}
	outputBuffers.pop_front();
}

void NdkVideoDecoder::processImages()
{
	if (!acquireFenceFd)
		return;
	ReflectedTexture& reflectedTexture = reflectedTextures[reflectedTextureIndex];
	if (!reflectedTexture.nextImage)
		return;
	acquireFenceFd = 0;

	//nextImage
	//__android_log_print(ANDROID_LOG_INFO, "NdkVideoDecoder", "NdkVideoDecoder - Succeeded");
	//..AImage_getHardwareBuffer
	// mInputSurface.makeCurrent();
	//__android_log_print(ANDROID_LOG_INFO, "NdkVideoDecoder", "output surface: await new image");
	//mOutputSurface.awaitNewImage();
	// Edit the frame and send it to the encoder.
	//__android_log_print(ANDROID_LOG_INFO, "NdkVideoDecoder", "output surface: draw image");
	//mOutputSurface.drawImage();
	// mInputSurface.setPresentationTime(info.presentationTimeUs * 1000);
	//__android_log_print(ANDROID_LOG_INFO, "NdkVideoDecoder", "input surface: swap buffers");
	//mInputSurface.swapBuffers();
	//__android_log_print(ANDROID_LOG_INFO, "NdkVideoDecoder", "video encoder: notified of new frame");
	//mInputSurface.releaseEGLContext();
}
