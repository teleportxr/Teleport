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

	Result VideoEncodePipeline::initialize(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::Node* videoOutput, avs::IOInterface* tagDataOutput)
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
		CrateEncodeParams(settings, videoEncodeParams, encoderParams);

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

	Result VideoEncodePipeline::reconfigure(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams)
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
		CrateEncodeParams(settings, videoEncodeParams, encoderParams);

		mEncoder->reconfigure(videoEncodeParams.encodeWidth, videoEncodeParams.encodeHeight, encoderParams);
		return Result::Code::OK;
	}

	void CrateEncodeParams(const CasterSettings& settings, const VideoEncodeParams& videoEncodeParams, avs::EncoderParams& encoderParams)
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
}