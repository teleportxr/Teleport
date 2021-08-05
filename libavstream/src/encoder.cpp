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

	d().m_videoData.resize(200000);
	d().m_tagData.resize(200);

	result = d().m_tagDataQueue.configure(200, 3, "VideoTagDataQueue");

	if (result)
	{
		StartEncodingThread();
	}

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

	result = d().ConfigureTagDataQueue();

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

	d().m_videoData.clear();
	d().m_tagData.clear();

	return d().m_tagDataQueue.deconfigure();
}

Result Encoder::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	// Next tell the backend encoder to actually encode a frame.
	assert(d().m_backend);
	return d().m_backend->encodeFrame(timestamp, d().m_forceIDR);
}

Result Encoder::writeOutput(const uint8_t* tagDataBuffer, size_t tagDataBufferSize)
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

	void* mappedBuffer;
	size_t mappedBufferSize;

	Result result = d().m_backend->mapOutputBuffer(mappedBuffer, mappedBufferSize);
	if (!result)
	{
		return result;
	}

	result = d().writeOutput(outputNode, mappedBuffer, mappedBufferSize, tagDataBuffer, tagDataBufferSize);
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
			size_t tagDataBufferSize;
			size_t bytesRead;
			result = d().m_tagDataQueue.read(this, d().m_tagData.data(), tagDataBufferSize, bytesRead);
			if (result == Result::IO_Empty)
			{
				tagDataBufferSize = 0;
			}
			else if (result == Result::IO_Retry)
			{
				d().m_tagData.resize(tagDataBufferSize);
				result = d().m_tagDataQueue.read(this, d().m_tagData.data(), tagDataBufferSize, bytesRead);
			}
			writeOutput(d().m_tagData.data(), tagDataBufferSize);
		}
	}
}

Result Encoder::writeTagData(const uint8_t* data, size_t dataSize)
{
	size_t bytesWritten;
	return d().m_tagDataQueue.write(this, data, dataSize, bytesWritten);
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

Result Encoder::Private::writeOutput(IOInterface* outputNode, const void* mappedBuffer, size_t mappedBufferSize, const uint8_t* tagDataBuffer, size_t tagDataBufferSize)
{
	assert(outputNode);
	assert(m_backend);

	// Write video and tag data to the output node
	size_t sizeFieldInBytes = sizeof(size_t);
	// 1 byte for payload type identification
	size_t tagDataSize = tagDataBufferSize + 1;

	size_t videoDataSize = tagDataSize + sizeFieldInBytes + mappedBufferSize;
	if (videoDataSize > m_videoData.size())
	{
		m_videoData.resize(videoDataSize);
	}

	size_t index = 0;
	memcpy(&m_videoData[index], &tagDataSize, sizeFieldInBytes);
	index += sizeFieldInBytes;
		
	if (m_params.codec == VideoCodec::H264)
	{
		// Aidan: I picked 30 (11110). See the classify function for the VideoPayloadType in nalu_parser_h264.hpp
		m_videoData[index++] = 30;
	}
	else if (m_params.codec == VideoCodec::HEVC)
	{
		// Aidan: I picked 62 (1 less than 111111). See the classify function for the VideoPayloadType in nalu_parser_h265.hpp
		// 62 << 1 is 124
		m_videoData[index++] = 62 << 1;
	}
	else
	{
		m_videoData[index++] = 0;
	}

	// Copy tag data 
	if (tagDataBufferSize > 0)
	{
		memcpy(&m_videoData[index], tagDataBuffer, tagDataBufferSize);
	}

	// Copy video encoder output data
	if (mappedBufferSize > 0)
	{
		memcpy(&m_videoData[index + tagDataBufferSize], mappedBuffer, mappedBufferSize);
	}

	size_t numBytesWrittenToOutput;
	Result result = outputNode->write(q_ptr(), m_videoData.data(), videoDataSize, numBytesWrittenToOutput);

	if (!result)
	{
		return result;
	}
		
	if (numBytesWrittenToOutput < videoDataSize)
	{
		AVSLOG(Warning) << "Encoder: Incomplete video frame written to output node";
		return Result::Encoder_IncompleteFrame;
	}

	return result;
}

Result Encoder::Private::ConfigureTagDataQueue()
{
	return m_tagDataQueue.configure(200, 3, "VideoTagDataQueue");
}
