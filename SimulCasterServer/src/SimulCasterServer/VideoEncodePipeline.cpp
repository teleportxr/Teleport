#include "VideoEncodePipeline.h"
#include "ErrorHandling.h"
#include <iostream>
#include <algorithm>
#include <set>

#include <libavstream/platforms/platform_windows.hpp>
#include <libavstream/libavstream.hpp>
#include <libavstream/common.hpp>
#include <libavstream/surfaces/surface_dx11.hpp>
#include <libavstream/surfaces/surface_dx12.hpp>


namespace SCServer
{
	static void CrateEncodeParams(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::EncoderParams& encoderParams);

	VideoEncodePipeline::~VideoEncodePipeline()
	{
	
	}

	Result VideoEncodePipeline::initialize(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::Node* output)
	{
		auto createSurfaceBackend = [](GraphicsDeviceType deviceType, void* resource)->avs::SurfaceBackendInterface*
		{
			avs::SurfaceBackendInterface* avsSurfaceBackend = nullptr;

#if PLATFORM_WINDOWS
			if (deviceType == GraphicsDeviceType::Direct3D11)
			{
				avsSurfaceBackend = new avs::SurfaceDX11(reinterpret_cast<ID3D11Texture2D*>(resource));
			}
			if (deviceType == GraphicsDeviceType::Direct3D12)
			{
				avsSurfaceBackend = new avs::SurfaceDX12(reinterpret_cast<ID3D12Resource*>(resource));
			}
#endif
			if (deviceType == GraphicsDeviceType::OpenGL)
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
		
		pipeline.reset(new avs::Pipeline);
		encoder.reset(new avs::Encoder);
		inputSurface.reset(new avs::Surface);

		if (!inputSurface->configure(avsSurfaceBackend))
		{
			TELEPORT_CERR << "Failed to configure input surface node \n";
			return Result::Code::InputSurfaceNodeConfigurationError;
		}

		avs::EncoderParams encoderParams = {};
		CrateEncodeParams(settings, videoEncodeParams, encoderParams);

		if (!encoder->configure(avs::DeviceHandle{ (avs::DeviceType)videoEncodeParams.deviceType, videoEncodeParams.deviceHandle }, videoEncodeParams.encodeWidth, videoEncodeParams.encodeHeight, encoderParams))
		{
			TELEPORT_CERR << "Failed to configure encoder node \n";
			return Result::Code::EncoderNodeConfigurationError;
		}

		if (!pipeline->link({ inputSurface.get(), encoder.get(), output }))
		{
			TELEPORT_CERR << "Error configuring the video encoding pipeline \n";
			return Result::Code::PipelineConfigurationError;
		}

		return Result::Code::OK;
	}

	Result VideoEncodePipeline::reconfigure(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams)
	{
		if (!pipeline)
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
			if (!encoder->unregisterSurface())
			{
				TELEPORT_CERR << "Error occured trying to unregister the surface \n";
				return Result::Code::InputSurfaceUnregistrationError;
			}
			changeSurfaceBackendResource(inputSurface->getBackendSurface(), videoEncodeParams.deviceType, videoEncodeParams.inputSurfaceResource);
		}

		avs::EncoderParams encoderParams = {};
		CrateEncodeParams(settings, videoEncodeParams, encoderParams);

		encoder->reconfigure(videoEncodeParams.encodeWidth, videoEncodeParams.encodeHeight, encoderParams);
		return Result::Code::OK;
	}

	void CrateEncodeParams(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::EncoderParams& encoderParams)
	{
		encoderParams.codec = settings.videoCodec;
		encoderParams.preset = avs::VideoPreset::HighQuality;
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

	Result VideoEncodePipeline::process(const uint8_t* extraData, size_t extraDataSize, bool forceIDR)
	{
		if (!pipeline)
		{
			TELEPORT_CERR << "Error video encode pipeline not initialized \n";
			return Result::Code::PipelineNotInitialized;
		}

		encoder->setForceIDR(forceIDR);

		avs::Result result = pipeline->process();

		if (!result)
		{
			TELEPORT_CERR << "Encode pipeline processing encountered an error \n";
			return Result::Code::PipelineProcessingError;
		}

		result = encoder->writeOutput(extraData, extraDataSize);

		if (!result)
		{
			TELEPORT_CERR << "Encode pipeline encountered an error trying to write output \n";
			return Result::Code::PipelineWriteOutputError;
		}

		return Result::Code::OK;
	}

	Result VideoEncodePipeline::release()
	{
		encoder.reset();
		inputSurface.reset();
		pipeline.reset();
		
		return Result::Code::OK;
	}
}