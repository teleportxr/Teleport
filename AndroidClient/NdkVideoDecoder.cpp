#include "NdkVideoDecoder.h"
#include <media/NdkMediaFormat.h>
#include <iostream>

NdkVideoDecoder::NdkVideoDecoder(VideoDecoder *d,int codecType)
{
	mDecoder= AMediaCodec_createDecoderByType(getCodecMimeType());
}
void NdkVideoDecoder::initialize(void*  SurfaceTexture, int frameWidth, int frameHeight)
{
	if(mDecoderConfigured)
	{
		std::cerr<<"VideoDecoder: Cannot initialize: already configured"<<std::endl;
		return;
	}

	AMediaFormat *format=AMediaFormat_new();
	//= AMediaFormat_createVideoFormat(getCodecMimeType, frameWidth, frameHeight)
	// Guessing the following is equivalent:
	AMediaFormat_setString(format,AMEDIAFORMAT_KEY_MIME,getCodecMimeType());
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_WIDTH, frameWidth);
	AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_MAX_HEIGHT, frameHeight);

	//surface.setOnFrameAvailableListener(this)
	AMediaCodec_configure(mDecoder,format,nullptr,nullptr,0);
	//mDecoder.configure(format, Surface(surface), null, 0)
	AMediaCodec_start(mDecoder);
//	mDecoder.start()

	mDecoderConfigured = true;
}

void NdkVideoDecoder::shutdown()
{
	if(!mDecoderConfigured)
	{
		std::cerr<<"VideoDecoder: Cannot shutdown: not configured"<<std::endl;
		return;
	}
	AMediaCodec_flush(mDecoder);
	//mDecoder.flush()
	AMediaCodec_stop(mDecoder);
	//mDecoder.stop()
	mDecoderConfigured = false;
	mDisplayRequests = 0;
}

bool NdkVideoDecoder::decode(std::vector<uint8_t> &buffer, int payloadTypeIndex, bool lastPayload)
{
	if(!mDecoderConfigured)
	{
		std::cerr<<"VideoDecoder: Cannot decode buffer: not configured"<<std::endl;
		return false;
	}

	PayloadType payloadType = getPayloadTypeFromIndex(payloadTypeIndex);
	int payloadFlags=0;
	switch(payloadType)
	{
		case PayloadType::VPS:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
		case PayloadType::PPS:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
		case PayloadType::SPS:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
		case PayloadType::ALE:payloadFlags=AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;break;
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
	ssize_t bufferId = queueInputBuffer(inputBuffer, payloadFlags);
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
		std::cerr<<"VideoDecoder: Cannot display output: not configured"<<std::endl;
		return false;
	}
	while(mDisplayRequests > 0)
	{
		if (releaseOutputBuffer(mDisplayRequests == 1) > -2)
			mDisplayRequests--;
	}
	return true;
}

ssize_t NdkVideoDecoder::queueInputBuffer(std::vector<uint8_t> &buffer, int flags) 
{
	ssize_t inputBufferID=AMediaCodec_dequeueInputBuffer(mDecoder,0);
	//val inputBufferID = mDecoder.dequeueInputBuffer(0) // microseconds
	if(inputBufferID >= 0)
	{
		size_t buffer_size=0;
		uint8_t *inputBuffer=AMediaCodec_getInputBuffer(mDecoder,inputBufferID,&buffer_size);
		//val inputBuffer = mDecoder.getInputBuffer(inputBufferID)
		if(inputBuffer)
		{
			memcpy(inputBuffer,buffer.data(),buffer.size());
			//inputBuffer?.clear();
			//inputBuffer?.put(buffer);
		}
		AMediaCodec_queueInputBuffer(mDecoder,inputBufferID,0,buffer.size(),0,flags);
		//mDecoder.queueInputBuffer(inputBufferID, 0, buffer.size, 0, flags)
	}
	else
	{
		//Log.w("RemotePlay", "VideoDecoder: Could not dequeue decoder input buffer")
	}
	return inputBufferID;
}

int NdkVideoDecoder::releaseOutputBuffer(bool render) 
{
	AMediaCodecBufferInfo bufferInfo = {0};
	ssize_t outputBufferID=AMediaCodec_dequeueOutputBuffer(mDecoder,&bufferInfo,0);
	//int outputBufferID = mDecoder.dequeueOutputBuffer(bufferInfo, 0)
	if(outputBufferID >= 0)
	{
		AMediaCodec_releaseOutputBuffer(mDecoder,outputBufferID,render);
		//mDecoder.releaseOutputBuffer(outputBufferID, render)
	}
	else
	{
		//Log.w("RemotePlay", "VideoDecoder: Could not dequeue decoder output buffer");
	}
	return outputBufferID;
}

VideoCodec NdkVideoDecoder::getCodecType ()
{
	switch(mCodecTypeIndex)
	{
		case 1:
			return VideoCodec::H264;
		case 2:
			return VideoCodec::H265;
		default:
			return VideoCodec::INVALID;
	}
}

const char *NdkVideoDecoder::getCodecMimeType ()
{
	switch(getCodecType())
	{
		case VideoCodec::H264:
			return "video/avc";
		case VideoCodec::H265:
			return "video/hevc";
		default:
			return "";
	};
}

PayloadType NdkVideoDecoder::getPayloadTypeFromIndex(int payloadTypeIndex)
{
	switch(payloadTypeIndex)
	{
		case 0:
			return PayloadType::FirstVCL;
		case 1:
			return PayloadType::VCL;
		case 2:
			return PayloadType::VPS;
		case 3:
			return PayloadType::SPS;
		case 4:
			return PayloadType::PPS;
		case 5:
			return PayloadType::ALE;
		case 6:
			return PayloadType::OtherNALUnit;
		case 7:
			return PayloadType::AccessUnit;
		default:
			return PayloadType::OtherNALUnit;
	}
}