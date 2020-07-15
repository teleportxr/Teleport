// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <encoder_p.hpp>
#include <encoders/enc_nvidia.hpp>

#include <libavstream/buffer.hpp>
#include <libavstream/surface.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

using namespace avs;

Encoder::Encoder(EncoderBackend backend)
	: Node(new Encoder::Private(this))
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
	return result;
}

Result Encoder::reconfigure(int frameWidth, int frameHeight, const EncoderParams& params)
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	assert(d().m_backend);
	Result result = d().m_backend->reconfigure(frameWidth, frameHeight, params);
	if (result)
	{
		d().m_params = params;
		SurfaceInterface* surface = dynamic_cast<SurfaceInterface*>(getInput(0));
		registerSurface(surface);
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
	if (d().m_backend)
	{
		unlinkInput();
		result = d().m_backend->shutdown();
	}
	d().m_configured = false;
	d().m_outputPending = false;

	return result;
}

Result Encoder::process(uint32_t timestamp)
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	// Next tell the backend encoder to actually encode a frame.
	assert(d().m_backend);
	return d().m_backend->encodeFrame(timestamp, d().m_forceIDR);
}

Result Encoder::writeOutput(const uint8_t* extraDataBuffer, size_t bufferSize)
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
	return d().writeOutput(outputNode, extraDataBuffer, bufferSize);
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

Result Encoder::onInputLink(int slot, Node* node)
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

Result Encoder::onOutputLink(int slot, Node* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "Encoder: Output node does not implement IO operations";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

void Encoder::onInputUnlink(int slot, Node* node)
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

Result Encoder::Private::writeOutput(IOInterface* outputNode, const uint8_t* extraDataBuffer, size_t bufferSize)
{
	assert(outputNode);
	assert(m_backend);

	Result result = Result::OK;

	// Write any extra data such as the camera transform
	{
		size_t sizeFieldInBytes = sizeof(size_t);
		// 1 byte for payload type identification
		size_t dataSize = bufferSize + 1;

		std::vector<uint8_t> extraData(dataSize + sizeFieldInBytes);

		uint8_t index = 0;
		memcpy(&extraData[index], &dataSize, sizeFieldInBytes);
		index += sizeFieldInBytes;
		
		if (m_params.codec == VideoCodec::H264)
		{
			// Aidan: I picked 30 (11110). See the classify function for the VideoPayloadType in nalu_parser_h264.hpp
			extraData[index++] = 30;
		}
		else if (m_params.codec == VideoCodec::HEVC)
		{
			// Aidan: I picked 62 (1 less than 111111). See the classify function for the VideoPayloadType in nalu_parser_h265.hpp
			// 62 << 1 is 124
			extraData[index++] = 62 << 1;
		}
		else
		{
			extraData[index++] = 0;
		}

		if (bufferSize > 0)
		{
			memcpy(&extraData[index], extraDataBuffer, bufferSize);
		}

		size_t numBytesWrittenToOutput;
		result = outputNode->write(q_ptr(), extraData.data(), extraData.size(), numBytesWrittenToOutput);

		if (!result)
		{
			return result;
		}
		
		if (numBytesWrittenToOutput < sizeof(Transform))
		{
			AVSLOG(Warning) << "Encoder: Incomplete frame camera transform written to output node";
			return Result::Encoder_IncompleteFrame;
		}
	}

	// Write the image
	{
		void*  mappedBuffer;
		size_t mappedBufferSize;

		result = m_backend->mapOutputBuffer(mappedBuffer, mappedBufferSize);
		if (!result)
		{
			return result;
		}

		size_t numBytesWrittenToOutput;
		
		result = outputNode->amend(q_ptr(), mappedBuffer, mappedBufferSize, numBytesWrittenToOutput);

		m_backend->unmapOutputBuffer();

		if (!result)
		{
			return result;
		}
		if (numBytesWrittenToOutput < mappedBufferSize)
		{
			AVSLOG(Warning) << "Encoder: Incomplete frame image written to output node";
			return Result::Encoder_IncompleteFrame;
		}
	}

	return result;
}

void Encoder::setForceIDR(bool forceIDR)
{
	d().m_forceIDR = forceIDR;
}
