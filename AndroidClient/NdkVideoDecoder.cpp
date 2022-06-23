#include "NdkVideoDecoder.h"
#include <media/NdkMediaFormat.h>
#include <android/hardware_buffer_jni.h>
#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.hpp>
#include <iostream>
#include "Platform/Vulkan/Texture.h"
#include <TeleportCore/ErrorHandling.h>
#include <android/log.h>
#define AMEDIA_CHECK(r) {media_status_t res=r;if(res!=AMEDIA_OK){TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;return;}}

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
	platform::vulkan::Texture *vulkanTexture=(platform::vulkan::Texture*)texture;
	AMediaFormat *format=AMediaFormat_new();
	//decoderParams.maxDecodePictureBufferCount
	AImageReader_newWithUsage(vulkanTexture->width,vulkanTexture->length,AIMAGE_FORMAT_RGBA_8888,AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,videoDecoderParams.maxDecodePictureBufferCount,&imageReader);
	// nativeWindow is managed by the ImageReader.
	ANativeWindow *nativeWindow=nullptr;
	media_status_t status;
	AMEDIA_CHECK(AImageReader_getWindow(imageReader,&nativeWindow));
	//= AMediaFormat_createVideoFormat(getCodecMimeType, frameWidth, frameHeight)
	// Guessing the following is equivalent:
	AMediaFormat_setString(format,AMEDIAFORMAT_KEY_MIME,getCodecMimeType());
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_WIDTH, vulkanTexture->width);
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_HEIGHT, vulkanTexture->length);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, vulkanTexture->length);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, vulkanTexture->width);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, videoDecoderParams.bitRate);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, videoDecoderParams.frameRate);
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
	/*
	//auto outp_format = AMediaCodec_getOutputFormat(mDecoder);
	AHardwareBuffer *hardwareBuffer=nullptr;
	AHardwareBuffer_Desc hardwareBuffer_Desc={};
	hardwareBuffer_Desc.format=AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
	hardwareBuffer_Desc.height=vulkanTexture->length;
	hardwareBuffer_Desc.width=vulkanTexture->width;
	hardwareBuffer_Desc.layers=1;
	hardwareBuffer_Desc.rfu0=0;
	hardwareBuffer_Desc.rfu1=0;
	hardwareBuffer_Desc.usage=AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
	
	AHardwareBuffer_allocate(&hardwareBuffer_Desc, &hardwareBuffer);
	VkAndroidHardwareBufferPropertiesANDROID androidHardwareBufferProperties;
	vk::Device *dev=renderPlatform->AsVulkanDevice();
	VkDevice vkDev=(VkDevice)dev;
	vkGetAndroidHardwareBufferPropertiesANDROID(vkDev,hardwareBuffer,&androidHardwareBufferProperties);
	VkMemoryGetAndroidHardwareBufferInfoANDROID androidHardwareBufferInfo;
	androidHardwareBufferInfo.sType=VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
	androidHardwareBufferInfo.memory=;
	vkGetMemoryAndroidHardwareBufferANDROID(vkDev,&androidHardwareBufferInfo,&hardwareBuffer);
	*/

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
			payloadFlags = 8;
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
	if ((bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0)
	{
		__android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder","video decoder: codec config buffer");
		AMediaCodec_releaseOutputBuffer(codec,outputBufferId,false);
		return;
	}
    //auto bufferFormat = AMediaCodec_getOutputFormat(codec,outputBufferId); // option A
    // bufferFormat is equivalent to mOutputFormat
    // outputBuffer is ready to be processed or rendered.
 	__android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder","Output available %d, size: %d",outputBufferId,bufferInfo->size);

	AMediaCodec_releaseOutputBuffer(codec,outputBufferId,true);

	// Does this mean we can now do AImageReader_acquireNextImage?
	AImage *nextImage=nullptr;
	media_status_t res=AImageReader_acquireNextImage(imageReader, &nextImage) ;
	
	if(res!=AMEDIA_OK)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;
		return ;
	}

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

int32_t NdkVideoDecoder::queueInputBuffer(std::vector<uint8_t> &buffer, int flags,bool send) 
{
#if 0
	size_t pos=inputBufferToBeQueued.size();
	inputBufferToBeQueued.resize(pos+buffer.size());
	memcpy(inputBufferToBeQueued.data()+pos,buffer.data(),buffer.size());
	nextPayloadFlags=flags;
#else
	bool lastpacket=send;
	send=false;
    //ByteBuffer inputBuffer = codec->getInputBuffer(inputBufferId);
	if(buffer.size()==0)
		return -1;
	// Returns the index of an input buffer to be filled with valid data or -1 if no such buffer is currently available.
	//ssize_t inputBufferID=AMediaCodec_dequeueInputBuffer(codec,0);
	//val inputBufferID = mDecoder.dequeueInputBuffer(0) // microseconds
	if(!nextInputBuffers.size())
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"Out of buffers"<<std::endl;

		return -1;
	}
	/*if(inputBufferToBeQueued.size())
	{
		size_t pos=inputBufferToBeQueued.size();
		inputBufferToBeQueued.resize(pos+buffer.size());
		memcpy(inputBufferToBeQueued.data()+pos,buffer.data(),buffer.size());
		nextPayloadFlags=flags;
		return -1;
	}*/
	InputBuffer &inputBuffer=nextInputBuffers[nextInputBufferIndex];
	
	{
		size_t buffer_size=0;
		size_t copiedSize=0;
		uint8_t *targetBufferData=AMediaCodec_getInputBuffer(mDecoder,inputBuffer.inputBufferId,&buffer_size);
		if(!targetBufferData)
		{
			TELEPORT_CERR<<"NdkVideoDecoder - "<<"AMediaCodec_getInputBuffer failed."<<std::endl;
			return -1;
		}
		copiedSize=std::min(buffer_size-inputBuffer.offset,buffer.size());
		bool add_to_new=!targetBufferData||copiedSize<buffer.size();
		// if buffer is valid, copy our data into it.
		if(targetBufferData&&copiedSize==buffer.size())
		{
			// if flags changes and data is already on the buffer, send this buffer and add to a new one.
			if(inputBuffer.flags!=-1&&flags!=inputBuffer.flags)
			{
				send=true;
				add_to_new=true;
			}
			else
			{
				memcpy(targetBufferData+inputBuffer.offset,buffer.data(),copiedSize);
				inputBuffer.offset+=copiedSize;
				inputBuffer.flags=flags;
				__android_log_print(ANDROID_LOG_INFO,"queueInputBuffer","buffer %d added: %zu bytes with flag %d",inputBuffer.inputBufferId,copiedSize,flags);
			}
		}
		else
		{
			//TELEPORT_COUT<<"queueInputBuffer - buffer "<<inputBuffer.inputBufferId<<" full."<<std::endl;
			__android_log_print(ANDROID_LOG_INFO,"queueInputBuffer","buffer %d full",inputBuffer.inputBufferId);
			send=true;
		}
		if(send&&inputBuffer.offset)
		{
			media_status_t res=AMediaCodec_queueInputBuffer(mDecoder,inputBuffer.inputBufferId,0,inputBuffer.offset,0,inputBuffer.flags);
			if(res!=AMEDIA_OK)
			{
				TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;
				return -1;
			}
			if(lastpacket)
				__android_log_print(ANDROID_LOG_INFO,"queueInputBuffer","Last Packet.");
			__android_log_print(ANDROID_LOG_INFO,"queueInputBuffer","buffer: %d SENT %zu bytes with flag %d.",inputBuffer.inputBufferId,inputBuffer.offset,inputBuffer.flags);
			//TELEPORT_COUT<<"queueInputBuffer - buffer "<<inputBuffer.inputBufferId<<" SENT "<<inputBuffer.offset<<" bytes with flag "<<inputBuffer.flags<<"."<<std::endl;
			nextInputBuffers.erase(nextInputBuffers.begin()+nextInputBufferIndex);
			//inputBuffer.offset=0;
			//inputBuffer.flags=-1;
			//nextInputBufferIndex++;
			if(nextInputBufferIndex>=nextInputBuffers.size())
				nextInputBufferIndex=0;
		}
		if(add_to_new)
		{
			InputBuffer &nextInputBuffer=nextInputBuffers[nextInputBufferIndex];
			uint8_t *targetBufferData2=AMediaCodec_getInputBuffer(mDecoder,nextInputBuffer.inputBufferId,&buffer_size);
			if(targetBufferData2)
			{
				__android_log_print(ANDROID_LOG_INFO,"queueInputBuffer","newbuffer %d added: %lu bytes with flag %d.",inputBuffer.inputBufferId,buffer.size(),flags);
				//TELEPORT_COUT<<"queueInputBuffer - next buffer "<<nextInputBuffer.inputBufferId<<" added "<<buffer.size()<<" bytes with flag "<<flags<<"."<<std::endl;
				memcpy(targetBufferData2,buffer.data(),buffer.size());
				nextInputBuffer.offset+=buffer.size();
				nextInputBuffer.flags=flags;
			}
			else
			{
				TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;
				return -1;
			}
		}
		// if any left over, put it in the next buffer.
		//nextInputBufferIds.erase(nextInputBufferIds.begin());
		return inputBuffer.inputBufferId;
	}
	//else
	{
		//Log.w("RemotePlay", "VideoDecoder: Could not dequeue decoder input buffer")
	}
   // codec.queueInputBuffer(inputBufferId, …);
#endif
	return -1;
}

int NdkVideoDecoder::releaseOutputBuffer(bool render) 
{
/*	AMediaCodecBufferInfo bufferInfo = {0};
	ssize_t outputBufferID=AMediaCodec_dequeueOutputBuffer(mDecoder,&bufferInfo,10000000);
	//int outputBufferID = mDecoder.dequeueOutputBuffer(bufferInfo, 0)
	if(outputBufferID >= 0)
	{
		AMediaCodec_releaseOutputBuffer(mDecoder,outputBufferID,render);
		//mDecoder.releaseOutputBuffer(outputBufferID, render)
	}
	else
	{
		//Log.w("RemotePlay", "VideoDecoder: Could not dequeue decoder output buffer");
	}*/
	return -1;// outputBufferID;
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
