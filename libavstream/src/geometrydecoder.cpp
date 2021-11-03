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
	struct GeometryDecoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(GeometryDecoder, PipelineNode)
		// non-owned backend
		GeometryDecoderBackendInterface *m_backend;
		std::unique_ptr<GeometryParserInterface> m_parser;

		std::vector<uint8_t> m_frameBuffer;
		NetworkFrameInfo m_frame;
		bool m_configured = false;
		int m_streamId = 0;
		Result processPayload(const uint8_t* buffer, size_t bufferSize, GeometryTargetInterface *target);
	};
} // avs

using namespace avs;

GeometryDecoder::GeometryDecoder()
	: PipelineNode(new GeometryDecoder::Private(this))
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
	d().m_frameBuffer.resize(2000);

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

Result GeometryDecoder::process(uint64_t timestamp, uint64_t deltaTime)
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

	IOInterface* input = dynamic_cast<IOInterface*>(getInput(0));
	if (!input)
	{
		return Result::Node_InvalidInput;
	}

	Result result = Result::OK;

	do
	{
		size_t bufferSize = d().m_frameBuffer.size();
		size_t bytesRead;
		result = input->read(this, d().m_frameBuffer.data(), bufferSize, bytesRead);

		if (result == Result::IO_Empty)
		{
			break;
		}

		if (result == Result::IO_Retry)
		{
			d().m_frameBuffer.resize(bufferSize);
			result = input->read(this, d().m_frameBuffer.data(), bufferSize, bytesRead);
		}

		if (result != Result::OK || bytesRead < sizeof(NetworkFrameInfo))
		{
			AVSLOG(Warning) << "GeometryDecoder: Failed to read input.";
			return result;
		}

		// Copy frame info 
		memcpy(&d().m_frame, d().m_frameBuffer.data(), sizeof(NetworkFrameInfo));

		// Check if data was lost or corrupted
		if (d().m_frame.broken)
		{
			AVSLOG(Warning) << "GeometryDecoder: Frame of size "<< d().m_frame.dataSize<<" was broken.\n";
			continue;
		}

		result = d().processPayload(d().m_frameBuffer.data() + sizeof(NetworkFrameInfo), d().m_frame.dataSize, gti);
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

Result GeometryDecoder::onInputLink(int slot, PipelineNode* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "GeometryDecoder: Input node must provide data";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

Result GeometryDecoder::onOutputLink(int slot, PipelineNode* node)
{
	if (!d().m_configured)
	{
		AVSLOG(Error) << "GeometryDecoder: PipelineNode needs to be configured before it can accept output";
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

void GeometryDecoder::onOutputUnlink(int slot, PipelineNode* node)
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