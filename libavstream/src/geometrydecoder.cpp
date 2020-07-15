// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd
#include <memory>
#include <vector>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/geometrydecoder.hpp>

namespace avs
{
	class GeometryParser final : public GeometryParserInterface
	{
	public:
		GeometryPayloadType classify(const uint8_t* buffer, size_t bufferSize, size_t& dataOffset) const override
		{
			assert(bufferSize >=sizeof(GeometryPayloadType));
			const uint8_t* data = buffer + dataOffset;
			GeometryPayloadType type;
			memcpy(&type, data,sizeof(GeometryPayloadType));
			dataOffset += sizeof(GeometryPayloadType);
			//Convert from raw number to payload type.
			return type;
		}
	};
	struct GeometryDecoder::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(GeometryDecoder, Node)
		// non-owned backend
		GeometryDecoderBackendInterface *m_backend;
		std::unique_ptr<GeometryParserInterface> m_parser;

		NetworkFrame m_frame;
		bool m_configured = false;
		int m_streamId = 0;
		Result processPayload(const uint8_t* buffer, size_t bufferSize, GeometryTargetInterface *target);
	};
} // avs

using namespace avs;

GeometryDecoder::GeometryDecoder()
	: Node(new GeometryDecoder::Private(this))
{
	setNumSlots(1, 1);
	d().m_parser.reset(new GeometryParser);
}

GeometryDecoder::~GeometryDecoder()
{
	deconfigure();
}

Result GeometryDecoder::configure(uint8_t streamId, GeometryDecoderBackendInterface* backend)
{
	if (d().m_configured)
	{
		Result deconf_result = deconfigure();
		if (deconf_result != Result::OK)
			return Result::Node_AlreadyConfigured;
	}
	d().m_backend=(backend);

	assert(d().m_backend);

	d().m_configured = true;
	d().m_streamId = streamId;
	return Result::OK;
}

Result GeometryDecoder::deconfigure()
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

//Result GeometryDecoder::decode(const NetworkFrame& frame)
//{
//	std::lock_guard<std::mutex> guard(d().m_mutex);
//
//	if (!d().m_configured)
//	{
//		return Result::Node_NotConfigured;
//	}
//
//	auto *gti = dynamic_cast<GeometryTargetInterface*>(getOutput(0));
//	if (!gti)
//	{
//		return Result::Node_InvalidOutput;
//	}
//
//	// Check if data was lost or corrupted
//	if (data.broken)
//	{
//		return Result::GeometryDecoder_InvalidPayload;
//	}
//
//	d().processPayload(data.buffer, data.bufferSize, gti);
//
//	return Result::OK;
//}

Result GeometryDecoder::process(uint32_t timestamp)
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}
	auto *gti = dynamic_cast<GeometryTargetInterface*>(getOutput(0));
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
			AVSLOG(Warning) << "GeometryDecoder: Failed to read input";
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

Result GeometryDecoder::setBackend(GeometryDecoderBackendInterface* backend)
{
	if (d().m_configured)
	{
		AVSLOG(Error) << "GeometryDecoder: Cannot set backend: already configured";
		return Result::Node_AlreadyConfigured;
	}

	d().m_backend=backend;
	return Result::OK;
}

Result GeometryDecoder::Private::processPayload(const uint8_t* buffer, size_t bufferSize, GeometryTargetInterface *target)
{
	assert(m_backend);
	Result result = Result::UnknownError;

	size_t payloadTypeOffset = 0;
	GeometryPayloadType payloadType = m_parser->classify(buffer, bufferSize, payloadTypeOffset);

	if (m_backend && bufferSize)
	{
		result = m_backend->decode(buffer + payloadTypeOffset, bufferSize - payloadTypeOffset, payloadType, target->getGeometryTargetBackendInterface());
	}
	return result;
}

Result GeometryDecoder::onInputLink(int slot, Node* node)
{
	if (!dynamic_cast<FrameInterface*>(node))
	{
		AVSLOG(Error) << "GeometryDecoder: Input node must implement packet operations";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

Result GeometryDecoder::onOutputLink(int slot, Node* node)
{
	if (!d().m_configured)
	{
		AVSLOG(Error) << "GeometryDecoder: Node needs to be configured before it can accept output";
		return Result::Node_NotConfigured;
	}
	assert(d().m_backend);

	GeometryTargetInterface* m = dynamic_cast<GeometryTargetInterface*>(node);
	if (!m)
	{
		AVSLOG(Error) << "GeometryDecoder: Output node is not a Mesh";
		return Result::Node_Incompatible;
	}
	return Result::OK;// d().m_backend->registerSurface(surface->getBackendSurface());
}

void GeometryDecoder::onOutputUnlink(int slot, Node* node)
{
	if (!d().m_configured)
	{
		return;
	}

	GeometryTargetInterface* m = dynamic_cast<GeometryTargetInterface*>(node);
	if (m)
	{
		assert(d().m_backend);
		//d().m_backend->unregisterSurface(surface->getBackendSurface());
	}
}

uint8_t GeometryDecoder::getStreamId() const
{
	return d().m_streamId;
}