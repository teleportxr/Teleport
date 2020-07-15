// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd
#include <memory>
#include <vector>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/geometryencoder.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include <libavstream/buffer.hpp>
namespace avs
{
	void ConvertTransform(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, Transform &transform)
	{
		ConvertPosition(fromStandard, toStandard, transform.position);
		ConvertRotation(fromStandard, toStandard, transform.rotation);
		ConvertPosition(fromStandard, toStandard, transform.scale);
	}
	void ConvertRotation(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec4 &rotation)
	{
		if (fromStandard == toStandard)
			return;
		if ((fromStandard&(avs::AxesStandard::LeftHanded)) != (toStandard & (avs::AxesStandard::LeftHanded)))
		{
			//rotation = { -rotation.x,-rotation.y,-rotation.z,rotation.w };
		}
		if (fromStandard == avs::AxesStandard::UnrealStyle)
		{
			if (toStandard == avs::AxesStandard::GlStyle)
			{
				rotation = { -rotation.y, -rotation.z, +rotation.x, rotation.w };
			}

			if (toStandard == avs::AxesStandard::EngineeringStyle)
			{
				rotation = { -rotation.y, -rotation.x,-rotation.z, rotation.w };
			}
		}
		else if (fromStandard == avs::AxesStandard::EngineeringStyle)
		{
			if (toStandard == avs::AxesStandard::UnrealStyle)
			{
				rotation = { -rotation.y, -rotation.x, -rotation.z, rotation.w };
			}
			else if (toStandard == avs::AxesStandard::UnityStyle)
			{
				rotation = { -rotation.x, -rotation.z, -rotation.y, rotation.w };
			}
		}
		else if (fromStandard == avs::AxesStandard::GlStyle)
		{
			if (toStandard == avs::AxesStandard::UnrealStyle)
			{
				rotation = { +rotation.z, -rotation.x, -rotation.y, rotation.w };
			}
			else if (toStandard == avs::AxesStandard::UnityStyle)
			{
				rotation = { -rotation.x, -rotation.y, rotation.z, rotation.w };
			}
		}
		else if (fromStandard == avs::AxesStandard::UnityStyle)
		{
			if (toStandard == avs::AxesStandard::GlStyle)
			{
				rotation = { -rotation.x, -rotation.y, rotation.z, rotation.w };
			}

			if (toStandard == avs::AxesStandard::EngineeringStyle)
			{
				rotation = { -rotation.x, -rotation.z, -rotation.y, rotation.w };
			}
		}
	}

	void ConvertPosition(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec3 &position)
	{
		if (fromStandard == toStandard)
		{
			return;
		}
		if (fromStandard == avs::AxesStandard::UnrealStyle)
		{
			if (toStandard == avs::AxesStandard::GlStyle)
			{
				position = { +position.y, +position.z, -position.x };
			}
			if (toStandard == avs::AxesStandard::EngineeringStyle)
			{
				position = { position.y, position.x,position.z };
			}
		}
		else if (fromStandard == avs::AxesStandard::UnityStyle)
		{
			if (toStandard == avs::AxesStandard::GlStyle)
			{
				position = { position.x, position.y, -position.z };
			}
			if (toStandard == avs::AxesStandard::EngineeringStyle)
			{
				position = { position.x, position.z, position.y };
			}
		}
		else if (fromStandard == avs::AxesStandard::EngineeringStyle)
		{
			if (toStandard == avs::AxesStandard::UnrealStyle)
			{
				position = { position.y, position.x,position.z };
			}
			else if (toStandard == avs::AxesStandard::UnityStyle)
			{
				position = { position.x, position.z, position.y };
			}
		}
		else if (fromStandard == avs::AxesStandard::GlStyle)
		{
			if (toStandard == avs::AxesStandard::UnrealStyle)
			{
				position = { -position.z, +position.x, +position.y };
			}
			else if (toStandard == avs::AxesStandard::UnityStyle)
			{
				position = { position.x, position.y, -position.z };
			}
		}
	}
	
	struct GeometryEncoder::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(GeometryEncoder, Node)
		bool m_configured = false;
		bool m_outputPending = false;

		Result process(uint32_t timestamp, GeometrySourceInterface *inputMesh,IOInterface *outputNode);

		Result writeOutput(IOInterface* outputNode);
		// Geom encoder doesn't own its backend.
		GeometryEncoderBackendInterface *m_backend;
	};

} // avs

using namespace avs;

GeometryEncoder::GeometryEncoder()
	: Node(new GeometryEncoder::Private(this))
{
	setNumSlots(1, 1);
}

GeometryEncoder::~GeometryEncoder()
{
	deconfigure();
}

Result GeometryEncoder::configure(GeometryEncoderBackendInterface *backend)
{
	if (d().m_configured)
	{
		return Result::Node_AlreadyConfigured;
	}
	d().m_configured = true;
	d().m_backend=backend;
	Result result = Result::OK;
	return result;
}

Result GeometryEncoder::deconfigure()
{
	if (!d().m_configured)
	{
		return Result::Node_NotConfigured;
	}

	Result result = Result::OK;
	{
		unlinkInput();
	}
	d().m_configured = false;
	d().m_outputPending = false;
	return result;
}

Result GeometryEncoder::process(uint32_t timestamp)
{
	avs::Node *input0 = getInput(0);
	if (!input0)
	{
		return Result::Node_InvalidInput;
	}
	IOInterface* outputNode = dynamic_cast<IOInterface*>(getOutput(0));
	if (!outputNode)
	{
		return Result::Node_InvalidOutput;
	}
	GeometrySourceInterface* geometrySourceInterface = dynamic_cast<GeometrySourceInterface*>(input0);
	return d().process(timestamp, geometrySourceInterface,outputNode);
}

Result GeometryEncoder::Private::process(uint32_t timestamp, GeometrySourceInterface *geometrySourceInterface,IOInterface* outputNode)
{
	if (!m_configured)
	{
		return Result::Node_NotConfigured;
	}
	// First, we write any PENDING output.
	if (m_outputPending)
	{
		m_outputPending = false;
		if (Result result = writeOutput(outputNode); !result)
		{
			return result;
		}
	}
	// Next tell the backend encoder to actually encode. It will store the data for later retrieval.
	if (Result result = m_backend->encode(timestamp, geometrySourceInterface->getGeometrySourceBackendInterface(),
		geometrySourceInterface->getGeometryRequesterBackendInterface()); !result)
	{
		return result;
	}
	{
		return writeOutput(outputNode);
	}
}


Result GeometryEncoder::onInputLink(int slot, Node* node)
{
	if (!d().m_configured)
	{
		AVSLOG(Error) << "GeometryEncoder: Node needs to be configured before it can accept input";
		return Result::Node_NotConfigured;
	}
	assert(d().m_backend);

	GeometrySourceInterface* meshInterface = dynamic_cast<GeometrySourceInterface*>(node);
	if (!meshInterface)
	{
		AVSLOG(Error) << "GeometryEncoder: Input node is not a mesh\n" ;
		return Result::Node_Incompatible;
	}

	GeometrySourceBackendInterface* mi = meshInterface->getGeometrySourceBackendInterface();
#if 0
	if (si && si->getFormat() != d().m_backend->getInputFormat())
	{
		AVSLOG(Error) << "GeometryEncoder: Input mesh format is not compatible with GeometryEncoder configuration\n";
		return Result::Node_Incompatible;
	}
#endif
	return Result::OK;
}

Result GeometryEncoder::onOutputLink(int slot, Node* node)
{
	if (!dynamic_cast<IOInterface*>(node))
	{
		AVSLOG(Error) << "GeometryEncoder: Output node does not implement IO operations";
		return Result::Node_Incompatible;
	}
	return Result::OK;
}

void GeometryEncoder::onInputUnlink(int slot, Node* node)
{
	if (!d().m_configured)
	{
		return;
	}
}

Result GeometryEncoder::Private::writeOutput(IOInterface* outputNode)
{
	assert(outputNode);
	assert(m_backend);
	void*  mappedBuffer;
	size_t mappedBufferSize;
	Result result =  m_backend->mapOutputBuffer(mappedBuffer, mappedBufferSize);
	// If failed, return.
	if (!result)
		return result;
	// If nothing to write, early-out.
	if (!mappedBufferSize)
		return result;
	size_t numBytesWrittenToOutput;
	result = outputNode->write(q_ptr(), mappedBuffer, mappedBufferSize, numBytesWrittenToOutput);

	m_backend->unmapOutputBuffer();

	if (!result)
	{
		return result;
	}
	if (numBytesWrittenToOutput < mappedBufferSize)
	{
		AVSLOG(Warning) << "GeometryEncoder: Incomplete data written to output node";
		return Result::GeometryEncoder_Incomplete;
	}

	return Result::OK;
}
