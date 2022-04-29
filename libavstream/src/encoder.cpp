// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include "encoder_p.hpp"
#include "encoders/enc_nvidia.hpp"

#include <libavstream/buffer.hpp>
#include <libavstream/surface.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

using namespace avs;

Encoder::Encoder(EncoderBackend backend)
	: PipelineNode(new Encoder::Private(this))
{
	d().m_selectedBackendType = backend;
	setNumSlots(1, 1);
}

Encoder::Encoder(EncoderBackendInterface* backend)
	: Encoder(EncoderBackend::Custom)
{
	d().m_backend.reset(backend);
}

Encoder::~Encoder()
{
	deconfigure();
}

Result Encoder::configure(const DeviceHandle& device, int frameWidth, int frameHeight, const EncoderParams& params)
{
	if (d().m_configured)
	{
		return Result::Node_AlreadyConfigured;
	}

	d().m_surfaceRegistered = false;

	auto constructBackendFromType = [this](EncoderBackend backendType) -> EncoderBackendInterface*
	{
		switch (backendType)
		{
#if !defined(PLATFORM_ANDROID)
		case EncoderBackend::NVIDIA:
			// BUG: This only checks if NVENC libs are present in the system, may fail if multiple GPUs are present.
			//      Replace with more robust (device ID?) checks.
			return EncoderNV::checkSupport() ? new EncoderNV : nullptr;
#endif // !PLATFORM_ANDROID
		default:
			return nullptr;
		}
	};

	if (d().m_backend)
	{
		assert(d().m_selectedBackendType == EncoderBackend::Custom);
	}
	else
	{
		EncoderBackendInterface* ei;
		assert(d().m_selectedBackendType != EncoderBackend::Custom);
		if (d().m_selectedBackendType == EncoderBackend::Any)
		{
			ei = constructBackendFromType(EncoderBackend::NVIDIA);
			if (!ei)
			{
				AVSLOG(Error) << "No suitable encoder backend found";
				return Result::Encoder_NoSuitableBackendFound;
			}
		}
		else
		{
			ei = constructBackendFromType(d().m_selectedBackendType);
			if (!ei)
			{
				AVSLOG(Error) << "The selected encoder backend is not supported by this system";
				return Result::Encoder_NoSuitableBackendFound;
			}
		}

		d().m_backend.reset(ei);
	}

	assert(d().m_backend);
	Result result = d().m_backend->initialize(device, frameWidth, frameHeight, params);
	if (result)
	{
		d().m_params = params;
		d().m_configured = true;
	}

	if (result)
	{
		StartEncodingThread();
	}

	d().m_stats = {};

	d().m_timer.Start();

	return result;
}

Result Encoder::reconfigure(int frameWidth, int frameHeight, const EncoderParams& params)
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	StopEncodingThread();

	assert(d().m_backend);
	Result result = d().m_backend->reconfigure(frameWidth, frameHeight, params);
	if (result)
	{
		d().m_params = params;
		SurfaceInterface* surface = dynamic_cast<SurfaceInterface*>(getInput(0));
		registerSurface(surface);
	}

	if (result)
	{
		StartEncodingThread();
	}

	return result;
}

Result Encoder::deconfigure()
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	Result result = Result::OK;

	StopEncodingThread();

	if (d().m_backend)
	{
		unlinkInput();
		result = d().m_backend->shutdown();
	}
	d().m_configured = false;
	d().m_outputPending = false;

	return result;
}

Result Encoder::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	double connectionTime_s = d().m_timer.GetElapsedTimeS();
	if (connectionTime_s)
	{
		std::lock_guard<std::mutex> lock(d().m_statsMutex);
		++d().m_stats.framesSubmitted;
		d().m_stats.framesSubmittedPerSec = float(d().m_stats.framesSubmitted / connectionTime_s);
	}

	// Next tell the backend encoder to actually encode a frame.
	assert(d().m_backend);
	Result result = d().m_backend->encodeFrame(timestamp, d().m_forceIDR);

	if (!result)
	{
		return result;
	}

	if (!isEncodingAsynchronously())
	{
		result = writeOutput();
	}

	return result;
}

Result Encoder::writeOutput()
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}
	IOInterface* outputNode = dynamic_cast<IOInterface*>(getOutput(0));
	if (!outputNode)
	{
		return Result::Node_InvalidOutput;
	}

	double connectionTimeS = d().m_timer.GetElapsedTimeS();
	if (connectionTimeS)
	{
		std::lock_guard<std::mutex> lock(d().m_statsMutex);
		++d().m_stats.framesEncoded;
		d().m_stats.framesEncodedPerSec = float(d().m_stats.framesEncoded / connectionTimeS);
	}

	void* mappedBuffer;
	size_t mappedBufferSize;

	Result result = d().m_backend->mapOutputBuffer(mappedBuffer, mappedBufferSize);
	if (!result)
	{
		return result;
	}

	result = d().writeOutput(outputNode, mappedBuffer, mappedBufferSize);
	if (!result)
	{
		return result;
	}

	return d().m_backend->unmapOutputBuffer();
}

void Encoder::writeOutputAsync()
{
	while (d().m_encodingThreadActive)
	{
		Result result = d().m_backend->waitForEncodingCompletion();
		if (result)
		{
			writeOutput();
		}
	}
}

Result Encoder::setBackend(EncoderBackendInterface* backend)
{
	if (d().m_configured)
	{
		AVSLOG(Error) << "Encoder: Cannot set backend: already configured";
		return Result::Node_AlreadyConfigured;
	}

	d().m_selectedBackendType = EncoderBackend::Custom;
	d().m_backend.reset(backend);
	return Result::OK;
}

Result Encoder::onInputLink(int slot, PipelineNode* node)
{
	if (!d().m_configured)
	{
		AVSLOG(Error) << "Encoder: Cannot link to input node: encoder not configured";
		return Result::Node_AlreadyConfigured;
	}
	assert(d().m_backend);

	SurfaceInterface* surface = dynamic_cast<SurfaceInterface*>(node);
	if (!surface)
	{
		AVSLOG(Error) << "Encoder: Input node is not a Surface";
		return Result::Node_Incompatible;
	}

	return registerSurface(surface);
}

Result Encoder::onOutputLink(int slot, PipelineNode* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "Encoder: Output node does not implement IO operations";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

void Encoder::onInputUnlink(int slot, PipelineNode* node)
{
	unregisterSurface();
}

Result Encoder::registerSurface(SurfaceInterface* surface)
{
	if (d().m_surfaceRegistered)
	{
		AVSLOG(Error) << "Encoder: Cannot register surface: a surface is already registered";
		return Result::Encoder_SurfaceAlreadyRegistered;
	}

	Result result = d().m_backend->registerSurface(surface->getBackendSurface());

	if (result)
	{
		d().m_surfaceRegistered = true;
	}

	return result;
}

Result Encoder::unregisterSurface()
{
	if (!d().m_configured)
	{
		AVSLOG(Error) << "Encoder: Cannot unregister surface: encoder not configured";
		return Result::Node_AlreadyConfigured;
	}

	if (!d().m_surfaceRegistered)
	{
		AVSLOG(Error) << "Encoder: Cannot unregister surface: no surface is registered";
		return Result::Encoder_SurfaceNotRegistered;
	}

	Result result = d().m_backend->unregisterSurface();

	if (result)
	{
		d().m_surfaceRegistered = false;
	}

	return result;
}

void Encoder::StartEncodingThread()
{
	if (d().m_params.useAsyncEncoding && !d().m_encodingThread.joinable())
	{
		d().m_encodingThreadActive = true;
		d().m_encodingThread = std::thread(&Encoder::writeOutputAsync, this);
	}
}

void Encoder::StopEncodingThread()
{
	if (d().m_params.useAsyncEncoding && d().m_encodingThread.joinable())
	{
		d().m_encodingThreadActive = false;
		d().m_encodingThread.join();
	}
}

void Encoder::setForceIDR(bool forceIDR)
{
	d().m_forceIDR = forceIDR;
}

bool Encoder::isEncodingAsynchronously()
{
	return d().m_params.useAsyncEncoding;
}

EncoderStats Encoder::getStats() const
{
	std::lock_guard<std::mutex> lock(d().m_statsMutex);
	return d().m_stats;
}

Result Encoder::getEncodeCapabilities(const DeviceHandle& device, const EncoderParams& params, avs::EncodeCapabilities& capabilities)
{
#if !defined(PLATFORM_ANDROID)
	 return EncoderNV::getEncodeCapabilities(device, params, capabilities);
#else
	 return Result::NotSupported;
#endif 
}

Result Encoder::Private::writeOutput(IOInterface* outputNode, const void* mappedBuffer, size_t mappedBufferSize)
{
	assert(outputNode);

	size_t numBytesWrittenToOutput;
	Result result = outputNode->write(q_ptr(), mappedBuffer, mappedBufferSize, numBytesWrittenToOutput);

	if (!result)
	{
		return result;
	}
		
	if (numBytesWrittenToOutput < mappedBufferSize)
	{
		AVSLOG(Warning) << "Encoder: Incomplete video frame written to output node";
		return Result::Encoder_IncompleteFrame;
	}

	return result;
}
