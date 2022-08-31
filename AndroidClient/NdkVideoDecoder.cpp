#include "NdkVideoDecoder.h"
#include <media/NdkMediaFormat.h>
#include <android/hardware_buffer_jni.h>
#include <iostream>
#include "Platform/Vulkan/Texture.h"
#include <TeleportCore/ErrorHandling.h>
#include <Platform/Vulkan/RenderPlatform.h>
#include <android/log.h>
#include <sys/prctl.h>
#include <fmt/core.h>
void SetThreadName( const char* threadName)
{
  prctl(PR_SET_NAME,threadName,0,0,0);
}
#define AMEDIA_CHECK(r) {media_status_t res=r;if(res!=AMEDIA_OK){TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;return;}}

// TODO: this should be a member variabls:
	std::mutex buffers_mutex;
	std::atomic<bool> stopProcessBuffersThread=false;

void codecOnAsyncInputAvailable(
	AMediaCodec *codec,
	void *userdata,
	int32_t index)
{
	NdkVideoDecoder *ndkVideoDecoder=(NdkVideoDecoder*)userdata;
	ndkVideoDecoder->onAsyncInputAvailable(codec,index);
}
/**
 * Called when an output buffer becomes available.
 * The specified index is the index of the available output buffer.
 * The specified bufferInfo contains information regarding the available output buffer.
 */
 void codecOnAsyncOutputAvailable(
	AMediaCodec *codec,
	void *userdata,
	int32_t index,
	AMediaCodecBufferInfo *bufferInfo)
{
	NdkVideoDecoder *ndkVideoDecoder=(NdkVideoDecoder*)userdata;
	ndkVideoDecoder->onAsyncOutputAvailable(codec,index,bufferInfo);
}
/**
 * Called when the output format has changed.
 * The specified format contains the new output format.
 */
 void codecOnAsyncFormatChanged(
	AMediaCodec *codec,
	void *userdata,
	AMediaFormat *format)
{
	NdkVideoDecoder *ndkVideoDecoder=(NdkVideoDecoder*)userdata;
	ndkVideoDecoder->onAsyncFormatChanged(codec,format);
}

void readerOnImageAvailable(void* context, AImageReader* reader)
{
	NdkVideoDecoder *ndkVideoDecoder=(NdkVideoDecoder*)context;
	ndkVideoDecoder->onAsyncImageAvailable(reader);
}

/**
 * Called when the MediaCodec encountered an error.
 * The specified actionCode indicates the possible actions that client can take,
 * and it can be checked by calling AMediaCodecActionCode_isRecoverable or
 * AMediaCodecActionCode_isTransient. If both AMediaCodecActionCode_isRecoverable()
 * and AMediaCodecActionCode_isTransient() return false, then the codec error is fatal
 * and the codec must be deleted.
 * The specified detail may contain more detailed messages about this error.
 */
 void codecOnAsyncError(
        AMediaCodec *codec,
        void *userdata,
        media_status_t error,
        int32_t actionCode,
        const char *detail)
{
	NdkVideoDecoder *ndkVideoDecoder=(NdkVideoDecoder*)userdata;
	ndkVideoDecoder->onAsyncError(codec,error,actionCode,detail);
}

NdkVideoDecoder::NdkVideoDecoder(VideoDecoderBackend *d,avs::VideoCodec codecType)
{
	videoDecoder=d;
	mCodecType=codecType;
	mDecoder= AMediaCodec_createDecoderByType(getCodecMimeType());
}

void NdkVideoDecoder::initialize(platform::crossplatform::RenderPlatform* p,platform::crossplatform::Texture* texture)
{
	if(mDecoderConfigured)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"VideoDecoder: Cannot initialize: already configured"<<std::endl;
		return;
	}
	renderPlatform=(platform::vulkan::RenderPlatform*)p;
	targetTexture=(platform::vulkan::Texture*)texture;
	AMediaFormat *format=AMediaFormat_new();
	//decoderParams.maxDecodePictureBufferCount
	AMEDIA_CHECK(AImageReader_newWithUsage(targetTexture->width,targetTexture->length,AIMAGE_FORMAT_YUV_420_888,AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,videoDecoderParams.maxDecodePictureBufferCount,&imageReader));
	// nativeWindow is managed by the ImageReader.
	ANativeWindow *nativeWindow=nullptr;
	media_status_t status;
	AMEDIA_CHECK(AImageReader_getWindow(imageReader,&nativeWindow));
	//= AMediaFormat_createVideoFormat(getCodecMimeType, frameWidth, frameHeight)
	// Guessing the following is equivalent:
	AMediaFormat_setString	(format,AMEDIAFORMAT_KEY_MIME		,getCodecMimeType());
	AMediaFormat_setInt32	(format,AMEDIAFORMAT_KEY_MAX_WIDTH	,targetTexture->width);
	AMediaFormat_setInt32	(format,AMEDIAFORMAT_KEY_MAX_HEIGHT	,targetTexture->length);
	AMediaFormat_setInt32	(format,AMEDIAFORMAT_KEY_HEIGHT		,targetTexture->length);
	AMediaFormat_setInt32	(format,AMEDIAFORMAT_KEY_WIDTH		,targetTexture->width);
	AMediaFormat_setInt32	(format,AMEDIAFORMAT_KEY_BIT_RATE	,videoDecoderParams.bitRate);
	AMediaFormat_setInt32	(format,AMEDIAFORMAT_KEY_FRAME_RATE	,videoDecoderParams.frameRate);
	//int OUTPUT_VIDEO_COLOR_FORMAT =
    //        MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface;
	//AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 21); // #21 COLOR_FormatYUV420SemiPlanar (NV12) 
	
	uint32_t flags = 0;
	//surface.setOnFrameAvailableListener(this)
	media_status_t res=AMediaCodec_configure(mDecoder,format,nativeWindow,nullptr,flags);
	if(res!=AMEDIA_OK)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;
		return;
	}
	AMediaCodecOnAsyncNotifyCallback callback;
      callback.onAsyncInputAvailable=codecOnAsyncInputAvailable;
      callback.onAsyncOutputAvailable=codecOnAsyncOutputAvailable;
      callback.onAsyncFormatChanged=codecOnAsyncFormatChanged;
      callback.onAsyncError=codecOnAsyncError;
	res= AMediaCodec_setAsyncNotifyCallback(
			mDecoder,
			callback,
			this);
	if(res!=AMEDIA_OK)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;
		return;
	}
	/*AMEDIA_CHECK( AMediaCodec_setOnFrameRenderedCallback(
					  mDecoder,
					  AMediaCodecOnFrameRendered callback,
					  void *userdata
					);*/
	//mDecoder.configure(format, Surface(surface), null, 0)
	AMEDIA_CHECK(AMediaCodec_start(mDecoder));
	int format_color;
	AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &format_color);
	TELEPORT_CERR<<"NdkVideoDecoder AMEDIAFORMAT_KEY_COLOR_FORMAT - "<<format_color<<std::endl;
	auto format2 = AMediaCodec_getOutputFormat(mDecoder);
	AMediaFormat_getInt32(format2, AMEDIAFORMAT_KEY_COLOR_FORMAT, &format_color);
	TELEPORT_CERR<<"NdkVideoDecoder AMEDIAFORMAT_KEY_COLOR_FORMAT - "<<format_color<<std::endl;
	
	int32_t format1=0;
	AMEDIA_CHECK(AImageReader_getFormat(imageReader,&format1));
	int32_t maxImages=0;
	AMEDIA_CHECK(AImageReader_getMaxImages(imageReader, &maxImages)) ;
	TELEPORT_CERR<<"NdkVideoDecoder maxImages - "<<maxImages<<std::endl;
	reflectedTextures.resize(maxImages);
	for(int i=0;i<maxImages;i++)
	{
		reflectedTextures[i].sourceTexture=(platform::vulkan::Texture*)renderPlatform->CreateTexture(fmt::format("video source {0}",i).c_str());
	}
	static AImageReader_ImageListener imageListener;
	imageListener.context=this;
	imageListener.onImageAvailable=readerOnImageAvailable;
	AMEDIA_CHECK(AImageReader_setImageListener(imageReader, &imageListener));
	stopProcessBuffersThread=false;
	processBuffersThread=new std::thread(&NdkVideoDecoder::processBuffersOnThread, this);
	mDecoderConfigured = true;
}
int VulkanFormatToHardwareBufferFormat(VkFormat v)
{
	switch(v)
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

void NdkVideoDecoder::shutdown()
{
	stopProcessBuffersThread=true;
	if(processBuffersThread)
	{
		while(!processBuffersThread->joinable())
		{
		}
		processBuffersThread->join();
		delete processBuffersThread;
		processBuffersThread=nullptr;
	}
	if(!mDecoderConfigured)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"VideoDecoder: Cannot shutdown: not configured"<<std::endl;
		return;
	}
	AImageReader_delete(imageReader);
	imageReader=nullptr;
	AMediaCodec_flush(mDecoder);
	//mDecoder.flush()
	AMediaCodec_stop(mDecoder);
	//mDecoder.stop()
	AMediaCodec_delete(mDecoder);
	mDecoder=nullptr;
	mDecoderConfigured = false;
	mDisplayRequests = 0;
}

bool NdkVideoDecoder::decode(std::vector<uint8_t> &buffer, avs::VideoPayloadType payloadType, bool lastPayload)
{
	if(!mDecoderConfigured)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"VideoDecoder: Cannot decode buffer: not configured"<<std::endl;
		return false;
	}

	int payloadFlags=0;
	switch(payloadType)
	{
		case avs::VideoPayloadType::VPS:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
		case avs::VideoPayloadType::PPS:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
		case avs::VideoPayloadType::SPS:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
		case avs::VideoPayloadType::ALE:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
		default:break;
	}
	std::vector<uint8_t> startCodes;
	switch(payloadFlags)
	{
		case AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG:
			startCodes={0, 0, 0, 1};
			break;
		default:
			startCodes={0, 0, 1};
			break;
	}

	if(!lastPayload)
	{
		// Signifies partial frame data. For all VCLs in a frame besides the last one. Needed for H264.
		if (payloadFlags == 0)
		{
			payloadFlags = AMEDIACODEC_BUFFER_FLAG_PARTIAL_FRAME;
		}
	}

	std::vector<uint8_t> inputBuffer = startCodes;
	inputBuffer.resize(inputBuffer.size()+buffer.size());
	memcpy(inputBuffer.data()+startCodes.size(),buffer.data(),buffer.size());
	
	// get(dest,offset,length)
	// copies length bytes from this buffer into the given array,
	// starting at the current position of this buffer and at the given offset in the array.
	// The position of this buffer is then incremented by length.
	// buffer.get(inputBuffer, startCodes.size, buffer.remaining());
	// memcpy(inputBuffer.data(),buffer.data()+startCodes.size(),buffer.size()-startCodes.size());
	int32_t bufferId = queueInputBuffer(inputBuffer, payloadFlags,lastPayload);
	if(lastPayload && bufferId >= 0)
	{
		++mDisplayRequests;
		return true;
	}

	return false;
}

bool NdkVideoDecoder::display()
{
	if(!mDecoderConfigured)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"VideoDecoder: Cannot display output: not configured"<<std::endl;
		return false;
	}
	while(mDisplayRequests > 0)
	{
		if (releaseOutputBuffer(mDisplayRequests == 1) > -2)
			mDisplayRequests--;
	}
	return true;
}

// callbacks:
void NdkVideoDecoder::onAsyncInputAvailable(AMediaCodec *codec,
													int32_t inputBufferId)
{
	TELEPORT_COUT<<"NdkVideoDecoder - "<<"New input buffer: "<<inputBufferId<<std::endl;
	if(codec==mDecoder)
		nextInputBuffers.push_back({inputBufferId,0,-1});
}

void NdkVideoDecoder::onAsyncOutputAvailable(AMediaCodec *codec,
											int32_t outputBufferId,
											AMediaCodecBufferInfo *bufferInfo)
{
	if(codec!=mDecoder)
		return;
	outputBuffers.push_back({outputBufferId,bufferInfo->offset,bufferInfo->size,bufferInfo->presentationTimeUs,bufferInfo->flags});
}

static bool memory_type_from_properties(vk::PhysicalDevice *gpu,uint32_t typeBits, vk::MemoryPropertyFlags requirements_mask, uint32_t *typeIndex)
{
	vk::PhysicalDeviceMemoryProperties memory_properties;
	gpu->getMemoryProperties(&memory_properties);
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


void NdkVideoDecoder::onAsyncImageAvailable(AImageReader *reader)
{
   // if (!reader)
		return;
	static int counter=0;
	reflectedTextureIndex++;
	if(reflectedTextureIndex>=reflectedTextures.size())
		reflectedTextureIndex=0;
	TELEPORT_CERR<<"NdkVideoDecoder - onAsyncImageAvailable "<< reflectedTextureIndex<<std::endl;
	ReflectedTexture &reflectedTexture=reflectedTextures[reflectedTextureIndex];
	
	int32_t format1=0;
	AMEDIA_CHECK(AImageReader_getFormat(imageReader,&format1));
	int32_t maxImages=0;
	AMEDIA_CHECK(AImageReader_getMaxImages(imageReader, &maxImages));

	// Does this mean we can now do AImageReader_acquireNextImage?
	acquireFenceFd=0;
	auto res=AImageReader_acquireLatestImage(imageReader, &reflectedTexture.nextImage) ;
	//auto res=AImageReader_acquireLatestImageAsync(imageReader, &nextImage,&acquireFenceFd) ;
	//
	if(res!=AMEDIA_OK)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"AImageReader_acquireLatestImage Failed "<<std::endl;
		FreeTexture(reflectedTextureIndex);
		reflectedTextureIndex--;
		return ;
	}
	std::unique_lock<std::mutex> lock( _mutex);
	// start freeing images after 12 frames.
	int idx=(reflectedTextureIndex+4)%maxImages;
	texturesToFree.push_back(idx);
	
	vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
	AHardwareBuffer *hardwareBuffer=nullptr;
	AMEDIA_CHECK(AImage_getHardwareBuffer(reflectedTexture.nextImage,&hardwareBuffer));
	auto vkd=renderPlatform->AsVulkanDevice()->operator VkDevice();
	vk::AndroidHardwareBufferPropertiesANDROID		properties;
	vk::AndroidHardwareBufferFormatPropertiesANDROID  formatProperties;
	properties.pNext=&formatProperties;
	vk::Result vkResult=vulkanDevice->getAndroidHardwareBufferPropertiesANDROID(hardwareBuffer, &properties);
	AHardwareBuffer_Desc hardwareBufferDesc{};
	AHardwareBuffer_describe(hardwareBuffer, &hardwareBufferDesc);
// have to actually CREATE a vkImage EVERY FRAME????
	
	// mTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkExternalFormatANDROID externalFormatAndroid{
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
		.pNext = nullptr,
		.externalFormat = formatProperties.externalFormat,
	};

	vk::ExternalMemoryImageCreateInfo externalMemoryImageCreateInfo;
	externalMemoryImageCreateInfo.pNext = &externalFormatAndroid;
	externalMemoryImageCreateInfo.handleTypes =vk::ExternalMemoryHandleTypeFlagBits::eAndroidHardwareBufferANDROID;
//		VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;


	vk::ImageCreateInfo imageCreateInfo		= vk::ImageCreateInfo();
	imageCreateInfo.pNext					= &externalMemoryImageCreateInfo;
	//imageCreateInfo.flags = vk::ImageCreateFlagBits::eu;
	imageCreateInfo.imageType				= vk::ImageType::e2D;
	imageCreateInfo.format					= vk::Format::eUndefined;//eG8B8R83Plane420UnormKHR eUndefined
	imageCreateInfo.extent					= vk::Extent3D((uint32_t)targetTexture->width,(uint32_t)targetTexture->length, 1);
	imageCreateInfo.mipLevels				= 1;
	imageCreateInfo.arrayLayers				= hardwareBufferDesc.layers;
	imageCreateInfo.samples					= vk::SampleCountFlagBits::e1;
	imageCreateInfo.tiling					= vk::ImageTiling::eOptimal;
	imageCreateInfo.usage					= vk::ImageUsageFlagBits::eSampled;//|vk::ImageUsageFlagBits::eTransferSrc;//VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
	imageCreateInfo.sharingMode				= vk::SharingMode::eExclusive;
	imageCreateInfo.queueFamilyIndexCount	= 0;
	imageCreateInfo.pQueueFamilyIndices		= nullptr;
	imageCreateInfo.initialLayout			= vk::ImageLayout::eUndefined;
	// Not allowed with external format: imageCreateInfo.flags=vk::ImageCreateFlagBits::eMutableFormat;//VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
	
	vulkanDevice->createImage(&imageCreateInfo, nullptr, &reflectedTexture.videoSourceVkImage);
	
	vk::PhysicalDeviceMemoryProperties memoryProperties;
	renderPlatform->GetVulkanGPU()->getMemoryProperties(&memoryProperties);
// Required for AHB images.
	vk::MemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo=vk::MemoryDedicatedAllocateInfo().setImage(reflectedTexture.videoSourceVkImage);

	vk::ImportAndroidHardwareBufferInfoANDROID hardwareBufferInfo=vk::ImportAndroidHardwareBufferInfoANDROID()
		.setBuffer(hardwareBuffer)
		.setPNext(&memoryDedicatedAllocateInfo);
	
	vk::MemoryRequirements mem_reqs;

	//vulkanDevice->getImageMemoryRequirements(videoSourceVkImage, &mem_reqs);
	mem_reqs.size=properties.allocationSize;
	mem_reqs.memoryTypeBits=properties.memoryTypeBits;
	vk::MemoryAllocateInfo mem_alloc_info=vk::MemoryAllocateInfo()
		.setAllocationSize(properties.allocationSize)
		.setMemoryTypeIndex(memoryProperties.memoryTypes[0].heapIndex)
		.setMemoryTypeIndex(1 << (__builtin_ffs(properties.memoryTypeBits) - 1))
		.setPNext(&hardwareBufferInfo);
	// overwrites the above
	memory_type_from_properties(renderPlatform->GetVulkanGPU(),mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal,
		&mem_alloc_info.memoryTypeIndex);
	SIMUL_VK_CHECK(vulkanDevice->allocateMemory(&mem_alloc_info, nullptr,&reflectedTexture.mMem));
 // Dedicated memory bindings require offset 0.
 // Returns void:
	vulkanDevice->bindImageMemory(reflectedTexture.videoSourceVkImage, reflectedTexture.mMem, /*offset*/ 0);
	
	TELEPORT_CERR<<"NdkVideoDecoder - "<<"AImageReader_acquireLatestImage Succeeded"<<std::endl;

    nextImageIndex=reflectedTextureIndex;
}

void NdkVideoDecoder::onAsyncFormatChanged(	AMediaCodec *codec,
												AMediaFormat *format)
{
	int format_color;
	//auto outp_format = AMediaCodec_getOutputFormat(mDecoder);
	AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &format_color);
	TELEPORT_COUT<<"NdkVideoDecoder - "<<"onAsyncFormatChanged: "<<format_color<<std::endl;
}

void NdkVideoDecoder::onAsyncError(AMediaCodec *codec,
									media_status_t error,
									int32_t actionCode,
									const char *detail)
{
	TELEPORT_CERR<<"NdkVideoDecoder - "<<"VideoDecoder: error: "<<detail<<std::endl;
}
#pragma clang optimize off

int32_t NdkVideoDecoder::queueInputBuffer(std::vector<uint8_t> &b, int flg,bool snd) 
{
    //ByteBuffer inputBuffer = codec->getInputBuffer(inputBufferId);
	buffers_mutex.lock();
	dataBuffers.push_back({b,flg,snd});
	buffers_mutex.unlock();
	return 0;
}

void NdkVideoDecoder::processBuffersOnThread()
{
	SetThreadName("processBuffersOnThread");
	while(!stopProcessBuffersThread)
	{
		buffers_mutex.lock();
		processInputBuffers();
		processOutputBuffers();
		processImages();
		buffers_mutex.unlock();
		std::this_thread::sleep_for(std::chrono::nanoseconds(10000));
	}
}

void NdkVideoDecoder::processInputBuffers()
{
	// Returns the index of an input buffer to be filled with valid data or -1 if no such buffer is currently available.
	//ssize_t inputBufferID=AMediaCodec_dequeueInputBuffer(codec,0);
	//val inputBufferID = mDecoder.dequeueInputBuffer(0) // microseconds
	if(!dataBuffers.size())
	{
		// Nothing to process.
		return;
	}
	if(!nextInputBuffers.size())
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"Out of buffers, queueing."<<std::endl;
		return;
	}
	DataBuffer &dataBuffer=dataBuffers[0];
	bool send=dataBuffer.send;
	bool lastpacket=dataBuffer.send;
	send=false;
	InputBuffer &inputBuffer=nextInputBuffers[nextInputBufferIndex];
	
	
	size_t buffer_size=0;
	size_t copiedSize=0;
	uint8_t *targetBufferData=AMediaCodec_getInputBuffer(mDecoder,inputBuffer.inputBufferId,&buffer_size);
	if(!targetBufferData)
	{
		__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","AMediaCodec_getInputBuffer failed.");
		return ;
	}
	copiedSize=std::min(buffer_size-inputBuffer.offset,dataBuffer.bytes.size());
	bool add_to_new=!targetBufferData||copiedSize<dataBuffer.bytes.size();
	// if buffer is valid, copy our data into it.
	if(targetBufferData&&copiedSize==dataBuffer.bytes.size())
	{
		// if flags changes and data is already on the buffer, send this buffer and add to a new one.
		if(inputBuffer.flags!=-1&&dataBuffer.flags!=inputBuffer.flags)
		{
			send=true;
			add_to_new=true;
		}
		else
		{
			memcpy(targetBufferData+inputBuffer.offset,dataBuffer.bytes.data(),copiedSize);
			inputBuffer.offset+=copiedSize;
			inputBuffer.flags=dataBuffer.flags;
			//__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","buffer %d added: %zu bytes with flag %d",inputBuffer.inputBufferId,copiedSize,dataBuffer.flags);
			send=true;
		}
	}
	else
	{
		__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","buffer %d full",inputBuffer.inputBufferId);
		send=true;
	}
	if(send&&inputBuffer.offset)
	{
		media_status_t res=AMediaCodec_queueInputBuffer(mDecoder,inputBuffer.inputBufferId,0,inputBuffer.offset,0,inputBuffer.flags);
		if(res!=AMEDIA_OK)
		{
			__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","AMediaCodec_getInputBuffer failed.");
			return ;
		}
		//if(lastpacket)
		//	__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","Last Packet.");
		//__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","buffer: %d SENT %zu bytes with flag %d.",inputBuffer.inputBufferId,inputBuffer.offset,inputBuffer.flags);
		nextInputBuffers.erase(nextInputBuffers.begin()+nextInputBufferIndex);
		
		if(nextInputBufferIndex>=nextInputBuffers.size())
			nextInputBufferIndex=0;
	}
	if(add_to_new)
	{
		InputBuffer &nextInputBuffer=nextInputBuffers[nextInputBufferIndex];
		uint8_t *targetBufferData2=AMediaCodec_getInputBuffer(mDecoder,nextInputBuffer.inputBufferId,&buffer_size);
		if(targetBufferData2)
		{
			//__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","newbuffer %d added: %lu bytes with flag %d.",inputBuffer.inputBufferId,dataBuffer.bytes.size(),dataBuffer.flags);
			memcpy(targetBufferData2,dataBuffer.bytes.data(),dataBuffer.bytes.size());
			nextInputBuffer.offset+=dataBuffer.bytes.size();
			nextInputBuffer.flags=dataBuffer.flags;
		}
		else
		{
			__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","AMediaCodec_getInputBuffer failed.");
			return ;
		}
	}
	dataBuffers.erase(dataBuffers.begin());
}

int NdkVideoDecoder::releaseOutputBuffer(bool render) 
{
	buffers_mutex.lock();
	if(!outputBuffers.size())
	{
		buffers_mutex.unlock();
		return 0;
	}
	buffers_mutex.unlock();
	return 0;
}

void NdkVideoDecoder::processOutputBuffers() 
{
	if(!outputBuffers.size())
		return;
	OutputBuffer &outputBuffer=outputBuffers[0];
	if ((outputBuffer.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0)
	{
		__android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder","video decoder: codec config buffer");
		AMediaCodec_releaseOutputBuffer(mDecoder,outputBuffer.outputBufferId,false);
		return ;
	}
    //auto bufferFormat = AMediaCodec_getOutputFormat(codec,outputBufferId); // option A
    // bufferFormat is equivalent to mOutputFormat
    // outputBuffer is ready to be processed or rendered.
 	//__android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder","Output available %d, size: %d",outputBuffer.outputBufferId,outputBuffer.size);
	
	bool render = outputBuffer.size != 0;
	if(AMediaCodec_releaseOutputBuffer(mDecoder,outputBuffer.outputBufferId,true)!=AMEDIA_OK)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"AMediaCodec_releaseOutputBuffer Failed"<<std::endl;
	}
	
	outputBuffers.erase(outputBuffers.begin());
	
}

void NdkVideoDecoder::processImages()
{
	if(!acquireFenceFd)
		return;
	ReflectedTexture &reflectedTexture=reflectedTextures[reflectedTextureIndex];
	if(!reflectedTexture.nextImage)
		return;
	acquireFenceFd=0;
//nextImage
		__android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder","NdkVideoDecoder - Succeeded");
	//..AImage_getHardwareBuffer
       // mInputSurface.makeCurrent();
		__android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder", "output surface: await new image");
        //mOutputSurface.awaitNewImage();
        // Edit the frame and send it to the encoder.
        __android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder", "output surface: draw image");
        //mOutputSurface.drawImage();
       // mInputSurface.setPresentationTime(info.presentationTimeUs * 1000);
        __android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder", "input surface: swap buffers");
        //mInputSurface.swapBuffers();
        __android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder", "video encoder: notified of new frame");
        //mInputSurface.releaseEGLContext();
}
#include "Platform/Vulkan/EffectPass.h"
void NdkVideoDecoder::FreeTexture(int index)
{
	if(index<0||index>=reflectedTextures.size())
		return;
	ReflectedTexture &reflectedTexture=reflectedTextures[index];
	if(reflectedTexture.nextImage)
	{
		//vk::Device *vulkanDevice=renderPlatform->AsVulkanDevice();
		renderPlatform->PushToReleaseManager(reflectedTexture.mMem);
		//reflectedTexture.sourceTexture->InvalidateDeviceObjects();
		AImage_delete(reflectedTexture.nextImage);
		reflectedTexture.nextImage=nullptr;
	}
}

void NdkVideoDecoder::CopyVideoTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext)
{
	if(!mDecoderConfigured)
	{
		return;
	}
	std::unique_lock<std::mutex> lock( _mutex);
	// start freeing images after 12 frames.
	for(int i=0;i<texturesToFree.size();i++)
	{
		FreeTexture(texturesToFree[i]);
	}
	texturesToFree.clear();
	if(!renderPlatform)
		return;
	#if 0
	if(nextImageIndex<0)
		return;
	ReflectedTexture &reflectedTexture=reflectedTextures[nextImageIndex];
	if(!reflectedTexture.nextImage)
		return;
	if(!reflectedTexture.sourceTexture->IsValid())
		return;
	nextImageIndex=-1;	platform::crossplatform::TextureCreate textureCreate;
	textureCreate.w=targetTexture->width;
	textureCreate.l=targetTexture->length;
	textureCreate.d=1;
	textureCreate.arraysize=1;
	textureCreate.mips=1;
	textureCreate.f=platform::crossplatform::PixelFormat::UNDEFINED;//targetTexture->pixelFormat;
	textureCreate.numOfSamples=1;
	textureCreate.make_rt=false;
	textureCreate.setDepthStencil=false;
	textureCreate.need_srv=true;
	textureCreate.cubemap=false;
	textureCreate.external_texture=(void*)reflectedTexture.videoSourceVkImage;
	textureCreate.forceInit=true;
	
	reflectedTexture.sourceTexture->InitFromExternalTexture(renderPlatform,&textureCreate);
	reflectedTexture.sourceTexture->AssumeLayout(vk::ImageLayout::eUndefined);
	#endif
	// Can't use CopyTexture because we can't use transfer_src.
	//renderPlatform->CopyTexture(deviceContext,targetTexture,sourceTexture);
	auto *effect		=renderPlatform->copyEffect;
	auto *effectPass	=(platform::vulkan::EffectPass *)effect->GetTechniqueByName("copy_2d_from_video")->GetPass(0);
	//effectPass->SetVideoSource(true);
	targetTexture->activateRenderTarget(deviceContext);
	auto srcResource=effect->GetShaderResource("SourceTex2");
	auto dstResource=effect->GetShaderResource("DestTex2");
	//renderPlatform->SetTexture(deviceContext,srcResource,reflectedTexture.sourceTexture);
	effect->SetUnorderedAccessView(deviceContext,dstResource,targetTexture);
	renderPlatform->ApplyPass(deviceContext,effectPass);
	int w=(targetTexture->width+7)/8;
	int l=(targetTexture->length+7)/8;
	renderPlatform->DispatchCompute(deviceContext,w,l,1);
	renderPlatform->UnapplyPass(deviceContext);
	targetTexture->deactivateRenderTarget(deviceContext);
}

const char *NdkVideoDecoder::getCodecMimeType()
{
	switch(mCodecType)
	{
		case avs::VideoCodec::H264:
			return "video/avc";
		case avs::VideoCodec::HEVC:
			return "video/hevc";
		default:
			return "Invalid";
	};
}
