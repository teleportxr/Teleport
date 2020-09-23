// libavstream
// (c) Copyright 2018-2020 Simul Software Ltd
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
	{
		if (d().m_configured)
		{
			Result deconf_result = deconfigure();
			if (deconf_result != Result::OK)
				return Result::Node_AlreadyConfigured;
		}

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

		AudioTargetInterface * ati = dynamic_cast<AudioTargetInterface*>(getOutput(0));
		if (!ati)
		{
			AVSLOG(Error) << "AudioDecoder: Output node is not an audio target interface";
			return Result::Node_Incompatible;
		}

		Result result = ati->getAudioTargetBackendInterface()->deconfigure();

		if (!result)
		{
			return result;
		}

		result = unlinkOutput();
		
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

		auto* ati = dynamic_cast<AudioTargetInterface*>(getOutput(0));
		if (!ati)
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

			result = d().processPayload(d().m_frame.buffer.data(), d().m_frame.bufferSize, ati);
		} while (result == Result::OK);

		return result;
	}

	Result AudioDecoder::Private::processPayload(const uint8_t* buffer, size_t bufferSize, AudioTargetInterface* target)
	{
		Result result = Result::UnknownError;

		// At the moment there is only one payload
		size_t payloadTypeOffset = 0;
		//AudioPayloadType payloadType = m_parser->classify(buffer, bufferSize, payloadTypeOffset);

		if (target && bufferSize)
		{
			result = target->getAudioTargetBackendInterface()->process(buffer + payloadTypeOffset, bufferSize - payloadTypeOffset, AudioPayloadType::Capture);
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

		AudioTargetInterface* ati = dynamic_cast<AudioTargetInterface*>(node);
		if (!ati)
		{
			AVSLOG(Error) << "AudioDecoder: Output node is not an audio target interface";
			return Result::Node_Incompatible;
		}
		return Result::OK;
	}

	void AudioDecoder::onOutputUnlink(int slot, Node* node)
	{
		
	}

	uint8_t AudioDecoder::getStreamId() const
	{
		return d().m_streamId;
	}
} // avs