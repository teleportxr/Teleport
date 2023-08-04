// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd
#include <memory>
#include <vector>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/geometrydecoder.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

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
	};
} // avs

using namespace avs;

GeometryDecoder::GeometryDecoder()
	: PipelineNode(new GeometryDecoder::Private(this))
{
	setNumSlots(1, 1);
	m_parser.reset(new GeometryParser);
}

GeometryDecoder::~GeometryDecoder()
{
	deconfigure();
}

Result GeometryDecoder::configure(uint8_t streamId, GeometryDecoderBackendInterface* backend)
{
	if (m_configured)
	{
		Result deconf_result = deconfigure();
		if (deconf_result != Result::OK)
			return Result::Node_AlreadyConfigured;
	}
	m_backend=(backend);

	assert(m_backend);

	m_configured = true;
	m_streamId = streamId;
	m_buffer.resize(2000);

	return Result::OK;
}

Result GeometryDecoder::deconfigure()
{
	if (!m_configured)
	{
		return Result::Node_NotConfigured;
	}

	Result result = Result::OK;
	if (m_backend)
	{
		unlinkOutput();
	}

	m_configured = false;

	return result;
}

Result GeometryDecoder::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (!m_configured)
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
		size_t bufferSize = m_buffer.size();
		size_t bytesRead;
		result = input->read(this, m_buffer.data(), bufferSize, bytesRead);

		if (result == Result::IO_Empty)
		{
			break;
		}

		if (result == Result::IO_Retry)
		{
			m_buffer.resize(bufferSize);
			result = input->read(this, m_buffer.data(), bufferSize, bytesRead);
		}

		if (result != Result::OK)
		{
			AVSLOG(Warning) << "GeometryDecoder: Failed to read input.";
			return result;
		}

		PayloadInfoType payloadInfoType = (PayloadInfoType)m_buffer[4];

		size_t dataOffset;
		size_t dataSize;
		GeometryPayloadType payloadType = GeometryPayloadType::Invalid;
		if (payloadInfoType == PayloadInfoType::Stream)
		{
			StreamPayloadInfo info;
			memcpy(&info, m_buffer.data(), sizeof(StreamPayloadInfo));

			// Check if data was lost or corrupted
			if (info.broken)
			{
				continue;
			}

			dataOffset = sizeof(StreamPayloadInfo);
			// The offset is incremented in the classify function to be after the payload type.
			payloadType = m_parser->classify(m_buffer.data(), bufferSize, dataOffset);

			dataSize = info.dataSize - sizeof(GeometryPayloadType);
		}
		else if (payloadInfoType == PayloadInfoType::File)
		{
			FilePayloadInfo info;
			memcpy(&info, m_buffer.data(), sizeof(FilePayloadInfo));

			dataOffset = sizeof(FilePayloadInfo);
			dataSize = info.dataSize;

			switch (info.httpPayloadType)
			{
			case FilePayloadType::Texture:
				payloadType = GeometryPayloadType::Texture;
				break;
			case FilePayloadType::Mesh:
				payloadType = GeometryPayloadType::Mesh;
				break;
			case FilePayloadType::Material:
				payloadType = GeometryPayloadType::Material;
				break;
			default:
				continue;
			}
		}
		else
		{
			continue;
		}
		// Next 8 bytes is the uid.
		if (dataSize < 8)
		{
			return Result::Failed;
		}
		uint8_t* ptr = m_buffer.data() + dataOffset;
		avs::uid uid = *((avs::uid*)ptr);
		dataSize -= sizeof(uid);
		ptr += sizeof(uid);
		if (dataSize==0)
		{
			return Result::Failed;
		}
		// TODO: this only supports one server.
		avs::uid server_uid=1;
		result = processPayload(server_uid,ptr, dataSize, payloadType, gti,uid);
	} while (result == Result::OK);


	return result;
}

Result GeometryDecoder::setBackend(GeometryDecoderBackendInterface* backend)
{
	if (m_configured)
	{
		AVSLOG(Error) << "GeometryDecoder: Cannot set backend: already configured";
		return Result::Node_AlreadyConfigured;
	}

	m_backend=backend;
	return Result::OK;
}

Result GeometryDecoder::processPayload(avs::uid server_uid,const uint8_t* buffer, size_t bufferSize, GeometryPayloadType payloadType, GeometryTargetInterface *target,avs::uid uid)
{
	assert(m_backend);
	Result result = Result::UnknownError;

	if (m_backend && bufferSize)
	{
		result = m_backend->decode(server_uid,buffer, bufferSize, payloadType, target->getGeometryTargetBackendInterface(),uid);
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
	if (!m_configured)
	{
		AVSLOG(Error) << "GeometryDecoder: PipelineNode needs to be configured before it can accept output";
		return Result::Node_NotConfigured;
	}
	assert(m_backend);

	GeometryTargetInterface* m = dynamic_cast<GeometryTargetInterface*>(node);
	if (!m)
	{
		AVSLOG(Error) << "GeometryDecoder: Output node is not a Mesh";
		return Result::Node_Incompatible;
	}
	return Result::OK;// m_backend->registerSurface(surface->getBackendSurface());
}

void GeometryDecoder::onOutputUnlink(int slot, PipelineNode* node)
{
	if (!m_configured)
	{
		return;
	}

	GeometryTargetInterface* m = dynamic_cast<GeometryTargetInterface*>(node);
	if (m)
	{
		assert(m_backend);
		//m_backend->unregisterSurface(surface->getBackendSurface());
	}
}

uint8_t GeometryDecoder::getStreamId() const
{
	return m_streamId;
}