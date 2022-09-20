// (C) Copyright 2018-2022 Simul Software Ltd

#include "VideoDecoderBackend.h"
#include "NdkVideoDecoder.h"
#include <TeleportCore/ErrorHandling.h>

using namespace teleport;
using namespace android;

VideoDecoderBackend::VideoDecoderBackend(platform::crossplatform::RenderPlatform* r, platform::crossplatform::Texture* t,DecodeEventInterface* eventInterface)
	:mEventInterface(eventInterface)
{
	renderPlatform=r;
	mColorSurfaceTexture=t;
}

VideoDecoderBackend::~VideoDecoderBackend()
{
	if(mColorDecoder)
	{
		delete mColorDecoder;
	}
	//if(mAlphaDecoder)
	{
	//	delete mAlphaDecoder;
	}
}

avs::Result VideoDecoderBackend::initialize(const avs::DeviceHandle& device, int frameWidth, int frameHeight, const avs::DecoderParams& params)
{
	if(mInitialized)
	{
		return avs::Result::OK;
	}

	mFrameWidth = frameWidth;
	mFrameHeight = frameHeight;

	mUseAlphaLayerDecoding = params.useAlphaLayerDecoding;


	mColorDecoder = new NdkVideoDecoder(this,params.codec);

	//if (mUseAlphaLayerDecoding)
	//{
	//	mAlphaDecoder = new NdkVideoDecoder(this,params.codec);
	//}

	mInitialized = true;

	return avs::Result::OK;
}

avs::Result VideoDecoderBackend::reconfigure(int frameWidth, int frameHeight, const avs::DecoderParams& params)
{
	if(!mInitialized)
	{
		return avs::Result::DecoderBackend_NotInitialized;
	}

	mInitialized = false;

	avs::DeviceHandle dummyHandle;

	return initialize(dummyHandle, frameWidth, frameHeight, params);
}

avs::Result VideoDecoderBackend::shutdown()
{
	if(!mInitialized)
	{
		return avs::Result::DecoderBackend_NotInitialized;
	}
	if(mColorSurfaceTexture)
	{
		ShutdownVideoDecoder();
	}

	mFrameWidth  = 0;
	mFrameHeight = 0;
	mInitialized = false;
	return avs::Result::OK;
}

avs::Result VideoDecoderBackend::registerSurface(const avs::SurfaceBackendInterface* colorSurface, const avs::SurfaceBackendInterface* alphaSurface)
{
	if (!mColorDecoder)
	{
		TELEPORT_CERR << "VideoDecoder: Decoder not initialized";
		return avs::Result::DecoderBackend_NotInitialized;
	}
	if (!colorSurface || !colorSurface->getResource())
	{
		TELEPORT_CERR << "VideoDecoder: Invalid surface handle";
		return avs::Result::DecoderBackend_InvalidSurface;
	}
	if (colorSurface->getWidth() != mFrameWidth || colorSurface->getHeight() != mFrameHeight)
	{
		TELEPORT_CERR << "VideoDecoder: Output surface dimensions do not match video frame dimensions";
		return avs::Result::DecoderBackend_InvalidSurface;
	}
 
	//mAlphaSurfaceTexture = alphaSurfaceTexture;
	mColorDecoder->Initialize(renderPlatform,mColorSurfaceTexture);
	return avs::Result::OK;
}

void VideoDecoderBackend::CopyVideoTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext)
{
	if(mColorDecoder)
		mColorDecoder->CopyVideoTexture(deviceContext);
}

avs::Result VideoDecoderBackend::unregisterSurface()
{
	if(!mColorSurfaceTexture)
	{
		return avs::Result::DecoderBackend_SurfaceNotRegistered;
	}

	ShutdownVideoDecoder();
	return avs::Result::OK;
}

avs::Result VideoDecoderBackend::decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, avs::VideoPayloadType payloadType, bool lastPayload)
{
	if(!mInitialized)
	{
		return avs::Result::DecoderBackend_NotInitialized;
	}
	if(!mColorSurfaceTexture)
	{
		return avs::Result::DecoderBackend_InvalidSurface;
	}

	std::vector<uint8_t> bf(bufferSizeInBytes);
	memcpy(bf.data(),buffer,bufferSizeInBytes);
	bool isReadyToDisplay = mColorDecoder->Decode(bf, payloadType, lastPayload);
	
	//if (mUseAlphaLayerDecoding)
	{
		std::vector<uint8_t> abf(alphaBufferSizeInBytes);
		//isReadyToDisplay &= mAlphaDecoder->decode(abf, payloadType, lastPayload);
	}

	return isReadyToDisplay ? avs::Result::DecoderBackend_ReadyToDisplay : avs::Result::OK;
}

avs::Result VideoDecoderBackend::display(bool showAlphaAsColor)
{
	if(!mInitialized)
	{
		return avs::Result::DecoderBackend_NotInitialized;
	}
	if(!mColorSurfaceTexture)
	{
		return avs::Result::DecoderBackend_InvalidSurface;
	}
	bool displayResult = mColorDecoder->Display();
	if (mUseAlphaLayerDecoding && displayResult)
	{
		//displayResult = mAlphaDecoder->display();
	}
	return displayResult ? avs::Result::OK : avs::Result::DecoderBackend_DisplayFailed;
}

void VideoDecoderBackend::NotifyFrameAvailable()
{
	if(mEventInterface)
	{
		mEventInterface->OnFrameAvailable();
	}
}

void VideoDecoderBackend::ShutdownVideoDecoder()
{
	mColorDecoder->Shutdown();
	delete mColorDecoder;
	mColorDecoder = nullptr;
	if (mUseAlphaLayerDecoding)
	{
	//	mAlphaDecoder->shutdown();
	//	delete mAlphaDecoder;
	//	mAlphaDecoder = nullptr;
	}
	mColorSurfaceTexture = nullptr;
	//mAlphaSurfaceTexture = nullptr;
}

