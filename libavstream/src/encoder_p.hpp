// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <memory>
#include <queue>
#include <thread>
#include <atomic>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/encoder.hpp>
#include "libavstream/encoders/enc_interface.h"
#include <libavstream/queue.hpp>
#include <libavstream/timer.hpp>

namespace avs
{
	struct Encoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(Encoder, PipelineNode)
		std::unique_ptr<EncoderBackendInterface> m_backend;
		EncoderBackend m_selectedBackendType;
		EncoderParams m_params = {};
		mutable std::mutex m_statsMutex;
		EncoderStats m_stats = {};
		Timer m_timer;
		std::atomic_bool m_encodingThreadActive;
		std::thread m_encodingThread;
		bool m_configured = false;
		bool m_surfaceRegistered = false;
		bool m_outputPending = false;
		bool m_forceIDR = false;

		Result writeOutput(IOInterface* outputNode, const void* mappedBuffer, size_t mappedBufferSize);
		Result ConfigureTagDataQueue();
	};

} // avs