#include "VideoEncodePipeline.h"
#include "TeleportCore/ErrorHandling.h"
#include <iostream>
#include <algorithm>
#include <set>

#include <libavstream/platforms/this_platform.h>
#include <libavstream/libavstream.hpp>
#include <libavstream/common.hpp>
#ifdef _MSC_VER
#include <libavstream/surfaces/surface_dx11.hpp>
#include <libavstream/surfaces/surface_dx12.hpp>
#endif

using namespace teleport;
using namespace server;

static void CrateEncodeParams(const ServerSettings& settings, avs::EncoderParams& encoderParams);

VideoEncodePipeline::~VideoEncodePipeline()
{
	deconfigure();
}

Result VideoEncodePipeline::initialize(const ServerSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::PipelineNode* videoOutput, avs::IOInterface* tagDataOutput)
{
	auto createSurfaceBackend = [](GraphicsDeviceType deviceType, void* resource)->avs::SurfaceBackendInterface*
	{
		avs::SurfaceBackendInterface* avsSurfaceBackend = nullptr;

#if PLATFORM_WINDOWS
		if (deviceType == GraphicsDeviceType::Direct3D11)
		{
			avsSurfaceBackend = new avs::SurfaceDX11(reinterpret_cast<ID3D11Texture2D*>(resource));
		}
		else if (deviceType == GraphicsDeviceType::Direct3D12)
		{
			avsSurfaceBackend = new avs::SurfaceDX12(reinterpret_cast<ID3D12Resource*>(resource));
		}
#endif
		 if (deviceType == GraphicsDeviceType::Vulkan)
		{
			// TODO: Implement
		}
		else if (deviceType == GraphicsDeviceType::OpenGL)
		{
			// TODO: Implement
		}

		return avsSurfaceBackend;
	};

	if (!videoEncodeParams.deviceHandle)
	{
		TELEPORT_CERR << "Graphics device provided is null \n";
		return Result::Code::InvalidGraphicsDevice;
	}

	avs::SurfaceBackendInterface* avsSurfaceBackend;
	if (videoEncodeParams.inputSurfaceResource)
	{
		avsSurfaceBackend = createSurfaceBackend(videoEncodeParams.deviceType, videoEncodeParams.inputSurfaceResource);
	}
	else
	{
		TELEPORT_CERR << "Failed to create encoder color input surface texture. Graphics resource provided is null. \n";
		return Result::Code::InvalidGraphicsResource;
	}

	mPipeline.reset(new avs::Pipeline);
	mEncoder.reset(new avs::Encoder);
	mInputSurface.reset(new avs::Surface);
	mTagDataOutput = tagDataOutput;

	if (!mInputSurface->configure(avsSurfaceBackend))
	{
		TELEPORT_CERR << "Failed to configure input surface node \n";
		return Result::Code::InputSurfaceNodeConfigurationError;
	}

	avs::EncoderParams encoderParams = {};
	CrateEncodeParams(settings, encoderParams);

	if (!mEncoder->configure(avs::DeviceHandle{ (avs::DeviceType)videoEncodeParams.deviceType, videoEncodeParams.deviceHandle }, videoEncodeParams.encodeWidth, videoEncodeParams.encodeHeight, encoderParams))
	{
		TELEPORT_CERR << "Failed to configure encoder node \n";
		return Result::Code::EncoderNodeConfigurationError;
	}

	if (!mPipeline->link({ mInputSurface.get(), mEncoder.get(), videoOutput }))
	{
		TELEPORT_CERR << "Error configuring the video encoding pipeline \n";
		return Result::Code::PipelineConfigurationError;
	}

	return Result::Code::OK;
}

Result VideoEncodePipeline::reconfigure1(const ServerSettings& settings, const VideoEncodeParams& videoEncodeParams)
{
	if (!mPipeline)
	{
		TELEPORT_CERR << "Error video encode pipeline not initialized \n";
		return Result::Code::PipelineNotInitialized;
	}

	auto changeSurfaceBackendResource = [](avs::SurfaceBackendInterface* avsSurfaceBackend, GraphicsDeviceType deviceType, void* resource)->void
	{
#if PLATFORM_WINDOWS
		if (deviceType == GraphicsDeviceType::Direct3D11)
		{
			(dynamic_cast<avs::SurfaceDX11*>(avsSurfaceBackend))->setResource(reinterpret_cast<ID3D11Texture2D*>(resource));
		}
		if (deviceType == GraphicsDeviceType::Direct3D12)
		{
			(dynamic_cast<avs::SurfaceDX12*>(avsSurfaceBackend))->setResource(reinterpret_cast<ID3D12Resource*>(resource));
		}
#endif
		if (deviceType == GraphicsDeviceType::OpenGL)
		{
			// TODO: Implement
		}
	};

	if (videoEncodeParams.inputSurfaceResource)
	{
		if (!mEncoder->unregisterSurface())
		{
			TELEPORT_CERR << "Error occured trying to unregister the surface \n";
			return Result::Code::InputSurfaceUnregistrationError;
		}
		changeSurfaceBackendResource(mInputSurface->getBackendSurface(), videoEncodeParams.deviceType, videoEncodeParams.inputSurfaceResource);
	}

	avs::EncoderParams encoderParams = {};
	CrateEncodeParams(settings, encoderParams);

	mEncoder->reconfigure(videoEncodeParams.encodeWidth, videoEncodeParams.encodeHeight, encoderParams);
	return Result::Code::OK;
}

void CrateEncodeParams(const ServerSettings& settings, avs::EncoderParams& encoderParams)
{
	encoderParams.codec = settings.videoCodec;
	encoderParams.preset = avs::VideoPreset::HighPerformance;
	encoderParams.targetFrameRate = settings.targetFPS;
	encoderParams.idrInterval = settings.idrInterval;
	encoderParams.rateControlMode = static_cast<avs::RateControlMode>(settings.rateControlMode);
	encoderParams.averageBitrate = settings.averageBitrate;
	encoderParams.maxBitrate = settings.maxBitrate;
	encoderParams.autoBitRate = settings.enableAutoBitRate;
	encoderParams.vbvBufferSizeInFrames = settings.vbvBufferSizeInFrames;
	encoderParams.deferOutput = settings.enableDeferOutput;
	encoderParams.useAsyncEncoding = settings.useAsyncEncoding;
	encoderParams.use10BitEncoding = settings.use10BitEncoding;
	encoderParams.useAlphaLayerEncoding = settings.useAlphaLayerEncoding;

	if (settings.use10BitEncoding)
	{
		encoderParams.inputFormat = avs::SurfaceFormat::ARGB10;
	}
	else
	{
		encoderParams.inputFormat = avs::SurfaceFormat::ARGB;
	}
}

Result VideoEncodePipeline::process(const uint8_t* tagData, size_t tagDataSize, bool forceIDR)
{
	if (!mPipeline)
	{
		TELEPORT_CERR << "Error video encode pipeline not initialized. \n";
		return Result::Code::PipelineNotInitialized;
	}

	size_t bytesWritten;
	mTagDataOutput->write(nullptr, tagData, tagDataSize, bytesWritten);

	if (!bytesWritten)
	{
		TELEPORT_CERR << "Failed to write tag data to output. \n";
		return Result::Code::PipelineProcessingError;
	}

	mEncoder->setForceIDR(forceIDR);

	avs::Result result = mPipeline->process();

	if (!result)
	{
		TELEPORT_CERR << "Encode pipeline processing encountered an error. \n";
		return Result::Code::PipelineProcessingError;
	}

	return Result::Code::OK;
}

Result VideoEncodePipeline::release()
{
	mEncoder.reset();
	mInputSurface.reset();
	mPipeline.reset();
	mTagDataOutput = nullptr;

	return Result::Code::OK;
}

avs::EncoderStats VideoEncodePipeline::getEncoderStats() const
{
	if (mEncoder)
		return mEncoder->getStats();
	return avs::EncoderStats();
}

Result VideoEncodePipeline::getEncodeCapabilities(const ServerSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::EncodeCapabilities& capabilities)
{
	avs::EncoderParams encoderParams = {};
	CrateEncodeParams(settings, encoderParams);
	if (avs::Encoder::getEncodeCapabilities(avs::DeviceHandle{ (avs::DeviceType)videoEncodeParams.deviceType, videoEncodeParams.deviceHandle }, encoderParams, capabilities))
	{
		return Result::Code::OK;
	}
	return Result::Code::EncodeCapabilitiesRetrievalError;
}


Result VideoEncodePipeline::configure(const ServerSettings& serverSettings, const VideoEncodeParams& videoEncodeParams, avs::Queue* colorQueue, avs::Queue* tagDataQueue)
{
	if (configured)
	{
		TELEPORT_CERR << "Video encode pipeline already configured." << "\n";
		return Result::Code::EncoderAlreadyConfigured;
	}

	if (!GraphicsManager::mGraphicsDevice)
	{
		TELEPORT_CERR << "Graphics device handle is null. Cannot attempt to initialize video encode pipeline." << "\n";
		return Result::Code::InvalidGraphicsDevice;
	}

	if (!videoEncodeParams.inputSurfaceResource)
	{
		TELEPORT_CERR << "Surface resource handle is null. Cannot attempt to initialize video encode pipeline." << "\n";
		return Result::Code::InvalidGraphicsResource;
	}

	inputSurfaceResource = videoEncodeParams.inputSurfaceResource;
	// Need to make a copy because Unity uses a typeless format which is not compatible with CUDA
	encoderSurfaceResource = GraphicsManager::CreateTextureCopy(inputSurfaceResource);

	VideoEncodeParams params = videoEncodeParams;
	params.deviceHandle = GraphicsManager::mGraphicsDevice;
	params.inputSurfaceResource = encoderSurfaceResource;

	Result result = teleport::server::VideoEncodePipeline::initialize(serverSettings, params, colorQueue, tagDataQueue);
	if (result)
	{
		configured = true;
	}
	return result;
}

Result VideoEncodePipeline::reconfigure(const ServerSettings& serverSettings, const VideoEncodeParams& videoEncodeParams)
{
	if (!configured)
	{
		TELEPORT_CERR << "Video encoder cannot be reconfigured if pipeline has not been configured." << "\n";
		return Result::Code::EncoderNotConfigured;
	}

	if (!GraphicsManager::mGraphicsDevice)
	{
		TELEPORT_CERR << "Graphics device handle is null. Cannot attempt to reconfigure video encode pipeline." << "\n";
		return Result::Code::InvalidGraphicsDevice;
	}

	if (videoEncodeParams.inputSurfaceResource)
	{
		TELEPORT_CERR << "Surface resource handle is null. Cannot attempt to reconfigure video encode pipeline." << "\n";
		return Result::Code::InvalidGraphicsResource;
	}

	VideoEncodeParams params = videoEncodeParams;
	params.deviceHandle = GraphicsManager::mGraphicsDevice;

	if (videoEncodeParams.inputSurfaceResource)
	{
		inputSurfaceResource = videoEncodeParams.inputSurfaceResource;
		// Need to make a copy because Unity uses a typeless format which is not compatible with CUDA
		encoderSurfaceResource = GraphicsManager::CreateTextureCopy(inputSurfaceResource);
		params.inputSurfaceResource = encoderSurfaceResource;
	}
	else
	{
		params.inputSurfaceResource = encoderSurfaceResource;
	}

	return server::VideoEncodePipeline::reconfigure1(serverSettings, params);
}

Result VideoEncodePipeline::encode(const uint8_t* tagData, size_t tagDataSize, bool forceIDR)
{
	if (!configured)
	{
		TELEPORT_CERR << "Video encoder cannot encode because it has not been configured." << "\n";
		return Result::Code::EncoderNotConfigured;
	}

	// Copy data from Unity texture to its CUDA compatible copy
	GraphicsManager::CopyResource(encoderSurfaceResource, inputSurfaceResource);

	return server::VideoEncodePipeline::process(tagData, tagDataSize, forceIDR);
}

Result VideoEncodePipeline::deconfigure()
{
	if (!configured)
	{
		TELEPORT_CERR << "Video encoder cannot be deconfigured because it has not been configured." << "\n";
		return Result::Code::EncoderNotConfigured;
	}

	Result result = release();
	if (!result)
	{
		return result;
	}

	GraphicsManager::ReleaseResource(encoderSurfaceResource);
	inputSurfaceResource = nullptr;

	configured = false;

	return result;
}