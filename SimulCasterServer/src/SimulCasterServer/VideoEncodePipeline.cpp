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
		release();
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
			std::cout << "Graphics device provided is null \n";
			return Result::InvalidGraphicsDevice;
		}

		avs::SurfaceBackendInterface* avsSurfaceBackend;
		if (videoEncodeParams.inputSurfaceResource)
		{
			avsSurfaceBackend = createSurfaceBackend(videoEncodeParams.deviceType, videoEncodeParams.inputSurfaceResource);
		}
		else
		{
			std::cout << "Failed to create encoder color input surface texture. Graphics resource provided is null. \n";
			return Result::InvalidGraphicsResource;
		}
		

		pipeline.reset(new avs::Pipeline);
		inputSurface.reset(new avs::Surface);
		encoder.reset(new avs::Encoder);

		if (!inputSurface->configure(avsSurfaceBackend))
		{
			std::cout << "Failed to configure input surface node \n";
			return Result::InputSurfaceNodeConfigurationError;
		}

		avs::EncoderParams encoderParams = {};
		CrateEncodeParams(settings, videoEncodeParams, encoderParams);

		if (!encoder->configure(avs::DeviceHandle{ (avs::DeviceType)videoEncodeParams.deviceType, videoEncodeParams.deviceHandle }, videoEncodeParams.encodeWidth, videoEncodeParams.encodeHeight, encoderParams))
		{
			std::cout << "Failed to configure encoder node \n";
			return Result::EncoderNodeConfigurationError;
		}

		if (!pipeline->link({ inputSurface.get(), encoder.get(), output }))
		{
			std::cout << "Error configuring the video encoding pipeline \n";
			return Result::PipelineConfigurationError;
		}

		return Result::OK;
	}

	Result VideoEncodePipeline::reconfigure(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams)
	{
		if (!pipeline)
		{
			std::cout << "Error video encode pipeline not initialized \n";
			return Result::PipelineNotInitialized;
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
				std::cout << "Error occured trying to unregister the surface \n";
				return Result::InputSurfaceUnregistrationError;
			}
			changeSurfaceBackendResource(inputSurface->getBackendSurface(), videoEncodeParams.deviceType, videoEncodeParams.inputSurfaceResource);
		}

		avs::EncoderParams encoderParams = {};
		CrateEncodeParams(settings, videoEncodeParams, encoderParams);

		encoder->reconfigure(videoEncodeParams.encodeWidth, videoEncodeParams.encodeHeight, encoderParams);
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
			std::cout << "Error video encode pipeline not initialized \n";
			return Result::PipelineNotInitialized;
		}

		encoder->setForceIDR(forceIDR);

		avs::Result result = pipeline->process();

		if (!result)
		{
			TELEPORT_CERR << "Encode pipeline processing encountered an error \n";
			return Result::PipelineProcessingError;
		}

		result = encoder->writeOutput(extraData, extraDataSize);

		if (!result)
		{
			TELEPORT_CERR << "Encode pipeline encountered an error trying to write output \n";
			return Result::PipelineWriteOutputError;
		}

		return Result::OK;
	}

	Result VideoEncodePipeline::release()
	{
		pipeline.reset();
		inputSurface.reset();
		encoder.reset();

		return Result::OK;
	}
}