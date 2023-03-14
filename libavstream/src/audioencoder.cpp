// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include "audioencoder_p.h"

#include <libavstream/buffer.hpp>

using namespace avs;

AudioEncoder::AudioEncoder(AudioEncoderBackend backend)
	: PipelineNode(new AudioEncoder::Private(this))
{
	d().m_selectedBackendType = backend;
	setNumSlots(0, 1);
}

AudioEncoder::AudioEncoder(AudioEncoderBackendInterface* backend)
	: AudioEncoder(AudioEncoderBackend::Custom)
{
	d().m_backend.reset(backend);
}

AudioEncoder::~AudioEncoder()
{
	deconfigure();
}

Result AudioEncoder::configure(const AudioEncoderParams& params)
{
	if (d().m_configured)
	{
		return Result::Node_AlreadyConfigured;
	}

	assert(d().m_backend);
	Result result = d().m_backend->initialize(params);
	if (result)
	{
		d().m_configured = true;
	}
	return result;
}

Result AudioEncoder::deconfigure()
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

	return result;
}

Result AudioEncoder::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	assert(d().m_backend);
	return d().m_backend->encode(timestamp, nullptr, 0);
}

Result AudioEncoder::writeOutput(const uint8_t* data, size_t dataSize)
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
	return d().writeOutput(outputNode, data, dataSize);
}

Result AudioEncoder::setBackend(AudioEncoderBackendInterface* backend)
{
	if (d().m_configured)
	{
		AVSLOG(Error) << "AudioEncoder: Cannot set backend: already configured";
		return Result::Node_AlreadyConfigured;
	}

	d().m_selectedBackendType = AudioEncoderBackend::Custom;
	d().m_backend.reset(backend);
	return Result::OK;
}

Result AudioEncoder::onInputLink(int slot, PipelineNode* node)
{
	if (!d().m_configured)
	{
		AVSLOG(Error) << "Encoder: Cannot link to input node: encoder not configured";
		return Result::Node_AlreadyConfigured;
	}
	assert(d().m_backend);

	return Result::OK;
}

Result AudioEncoder::onOutputLink(int slot, PipelineNode* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "AudioEncoder: Output node does not implement IO operations";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

void AudioEncoder::onInputUnlink(int slot, PipelineNode* node)
{
	
}

Result AudioEncoder::Private::writeOutput(IOInterface* outputNode, const uint8_t* data, size_t dataSize)
{
	assert(outputNode);
	assert(m_backend);

	size_t numBytesWrittenToOutput;
	Result result = Result::OK;
#ifdef FIX_BROKEN
	result= outputNode->write(q_ptr(), data, dataSize, numBytesWrittenToOutput);
#endif
	if (!result)
	{
		return result;
	}

	if (numBytesWrittenToOutput < dataSize)
	{
		AVSLOG(Warning) << "AudioEncoder: Incomplete write of current frame's audio data to output node";
		return Result::Encoder_IncompleteFrame;
	}

	return result;
}
