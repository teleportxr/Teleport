// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <memory>
#include <queue>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/encoder.hpp>
#include <libavstream/encoders/enc_interface.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

namespace avs
{
	struct Encoder::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(Encoder, Node)
		std::unique_ptr<EncoderBackendInterface> m_backend;
		EncoderBackend m_selectedBackendType;
		EncoderParams m_params = {};
		bool m_configured = false;
		bool m_surfaceRegistered = false;
		bool m_outputPending = false;
		bool m_forceIDR = false;

		Result writeOutput(IOInterface* outputNode, const uint8_t* tagDataBuffer, size_t tagDataBufferSize);
	};

} // avs