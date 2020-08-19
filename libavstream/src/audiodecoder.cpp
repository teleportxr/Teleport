// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd
#include <memory>
#include <vector>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/audiodecoder.h>

namespace avs
{
	class AudioParser final : public AudioParserInterface
	{
	public:
		AudioPayloadType classify(const uint8_t* buffer, size_t bufferSize, size_t& dataOffset) const override
		{
			assert(bufferSize >= sizeof(AudioPayloadType));
			const uint8_t* data = buffer + dataOffset;
			AudioPayloadType type;
			memcpy(&type, data, sizeof(AudioPayloadType));
			dataOffset += sizeof(AudioPayloadType);
			//Convert from raw number to payload type.
			return type;
		}
	};
	struct AudioDecoder::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(AudioDecoder, Node)
			// non-owned backend
			AudioDecoderBackendInterface* m_backend;
		std::unique_ptr<AudioParserInterface> m_parser;

		NetworkFrame m_frame;
		bool m_configured = false;
		int m_streamId = 0;
		Result processPayload(const uint8_t* buffer, size_t bufferSize, AudioTargetInterface* target);
	};

	AudioDecoder::AudioDecoder()
		: Node(new AudioDecoder::Private(this))
	{
		setNumSlots(1, 1);
		d().m_parser.reset(new AudioParser);
	}

	AudioDecoder::~AudioDecoder()
	{
		deconfigure();
	}

	Result AudioDecoder::configure(uint8_t streamId)
		if (d().m_configured)
		{
			Result deconf_result = deconfigure();
			if (deconf_result != Result::OK)
				return Result::Node_AlreadyConfigured;
		}
		d().m_backend = (backend);

		assert(d().m_backend);

		d().m_configured = true;
		d().m_streamId = streamId;
		return Result::OK;
	}

	Result AudioDecoder::deconfigure()
	{
		if (!d().m_configured)
		{
			return Result::Node_NotConfigured;
		}

		Result result = Result::OK;
		if (d().m_backend)
		{
			unlinkOutput();
		}

		d().m_configured = false;

		d().m_frame = {};

		return result;
	}

	Result AudioDecoder::process(uint32_t timestamp)
	{
		if (!d().m_configured)
		{
			return Result::Node_NotConfigured;
		}

		auto* gti = dynamic_cast<AudioTargetInterface*>(getOutput(0));
		if (!gti)
		{
			return Result::Node_InvalidOutput;
		}

		FrameInterface* input = dynamic_cast<FrameInterface*>(getInput(0));
		if (!input)
		{
			return Result::Node_InvalidInput;
		}

		Result result = Result::OK;

		do
		{
			result = input->readFrame(this, d().m_frame, d().m_streamId);

			if (result == Result::IO_Empty)
			{
				break;
			}
			else if (result != Result::OK)
			{
				AVSLOG(Warning) << "AudioDecoder: Failed to read input";
				return result;
			}

			// Check if data was lost or corrupted
			if (d().m_frame.broken)
			{
				continue;
			}

			result = d().processPayload(d().m_frame.buffer.data(), d().m_frame.bufferSize, gti);
		} while (result == Result::OK);


		return result;
	}

	Result AudioDecoder::Private::processPayload(const uint8_t* buffer, size_t bufferSize, AudioTargetInterface* target)
	{
		assert(m_backend);
		Result result = Result::UnknownError;

		// At the moment there is only one payload
		//size_t payloadTypeOffset = 0;
		//AudioPayloadType payloadType = m_parser->classify(buffer, bufferSize, payloadTypeOffset);

		if (target && bufferSize)
		{
			result = target->process(buffer + payloadTypeOffset, bufferSize - payloadTypeOffset, AudioPayloadType::Capture);
		}
		return result;
	}

	Result AudioDecoder::onInputLink(int slot, Node* node)
	{
		if (!dynamic_cast<FrameInterface*>(node))
		{
			AVSLOG(Error) << "AudioDecoder: Input node must implement packet operations";
			return Result::Node_Incompatible;
		}
		return Result::OK;
	}

	Result AudioDecoder::onOutputLink(int slot, Node* node)
	{
		if (!d().m_configured)
		{
			AVSLOG(Error) << "AudioDecoder: Node needs to be configured before it can accept output";
			return Result::Node_NotConfigured;
		}
		assert(d().m_backend);

		AudioTargetInterface* m = dynamic_cast<AudioTargetInterface*>(node);
		if (!m)
		{
			AVSLOG(Error) << "AudioDecoder: Output node is not a Mesh";
			return Result::Node_Incompatible;
		}
		return Result::OK;// d().m_backend->registerSurface(surface->getBackendSurface());
	}

	void AudioDecoder::onOutputUnlink(int slot, Node* node)
	{
		if (!d().m_configured)
		{
			return;
		}

		AudioTargetInterface* m = dynamic_cast<AudioTargetInterface*>(node);
		if (m)
		{
			assert(d().m_backend);
			//d().m_backend->unregisterSurface(surface->getBackendSurface());
		}
	}

	uint8_t AudioDecoder::getStreamId() const
	{
		return d().m_streamId;
	}
} // avs