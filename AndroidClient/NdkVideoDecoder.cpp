#include "NdkVideoDecoder.h"
#include <media/NdkMediaFormat.h>
#include <android/hardware_buffer_jni.h>
#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.hpp>
#include <iostream>
#include "Platform/Vulkan/Texture.h"
#include <TeleportCore/ErrorHandling.h>
#include <android/log.h>
#include <sys/prctl.h>
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
	AImageReader_newWithUsage(vulkanTexture->width,vulkanTexture->length,AIMAGE_FORMAT_RAW_PRIVATE,AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,videoDecoderParams.maxDecodePictureBufferCount,&imageReader);
	// nativeWindow is managed by the ImageReader.
	ANativeWindow *nativeWindow=nullptr;
	media_status_t status;
	AMEDIA_CHECK(AImageReader_getWindow(imageReader,&nativeWindow));
	//= AMediaFormat_createVideoFormat(getCodecMimeType, frameWidth, frameHeight)
	// Guessing the following is equivalent:
	AMediaFormat_setString(format,AMEDIAFORMAT_KEY_MIME,getCodecMimeType());
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_WIDTH, vulkanTexture->width);
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_HEIGHT, vulkanTexture->length);
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_HEIGHT, vulkanTexture->length);
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_WIDTH, vulkanTexture->width);
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_BIT_RATE, videoDecoderParams.bitRate);
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_FRAME_RATE, videoDecoderParams.frameRate);
	int OUTPUT_VIDEO_COLOR_FORMAT =
            MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface;
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 21); // #21 COLOR_FormatYUV420SemiPlanar (NV12) 
	
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
	auto format2 = AMediaCodec_getOutputFormat(mDecoder);
	AMediaFormat_getInt32(format2, AMEDIAFORMAT_KEY_COLOR_FORMAT, &format_color);
	
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
	while(!processBuffersThread->joinable())
	{
	}
	processBuffersThread->join();
	delete processBuffersThread;
	processBuffersThread=nullptr;
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
		//TELEPORT_CERR<<"NdkVideoDecoder - "<<"Out of buffers, queueing."<<std::endl;
		return;
	}
	if(!nextInputBuffers.size())
	{
		//TELEPORT_CERR<<"NdkVideoDecoder - "<<"Out of buffers, queueing."<<std::endl;
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
			__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","buffer %d added: %zu bytes with flag %d",inputBuffer.inputBufferId,copiedSize,dataBuffer.flags);
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
		if(lastpacket)
			__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","Last Packet.");
		__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","buffer: %d SENT %zu bytes with flag %d.",inputBuffer.inputBufferId,inputBuffer.offset,inputBuffer.flags);
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
			__android_log_print(ANDROID_LOG_INFO,"processInputBuffers","newbuffer %d added: %lu bytes with flag %d.",inputBuffer.inputBufferId,dataBuffer.bytes.size(),dataBuffer.flags);
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
 	__android_log_print(ANDROID_LOG_INFO,"NdkVideoDecoder","Output available %d, size: %d",outputBuffer.outputBufferId,outputBuffer.size);

	AMediaCodec_releaseOutputBuffer(mDecoder,outputBuffer.outputBufferId,true);
	
	outputBuffers.erase(outputBuffers.begin());
	// Does this mean we can now do AImageReader_acquireNextImage?
	AImage *nextImage=nullptr;
	media_status_t res=AImageReader_acquireLatestImage(imageReader, &nextImage) ;
	//AImage_getHardwareBuffer
	if(res!=AMEDIA_OK)
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"Failed"<<std::endl;
		return ;
	}
	else
	{
		TELEPORT_CERR<<"NdkVideoDecoder - "<<"Succeeded"<<std::endl;
		return ;
	}
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
