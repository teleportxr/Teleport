// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <memory>
#include <queue>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/audioencoder.h>
#include <libavstream/audio/audio_interface.h>

namespace avs
{
	struct AudioEncoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(AudioEncoder, PipelineNode)
		std::unique_ptr<AudioEncoderBackendInterface> m_backend;
		AudioEncoderBackend m_selectedBackendType;
		AudioEncoderParams m_params = {};
		bool m_configured = false;

		Result writeOutput(IOInterface* outputNode, const uint8_t* data, size_t dataSize);
	};

} // avs