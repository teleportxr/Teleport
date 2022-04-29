// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include "packetizer_p.hpp"
#include <libavstream/stream/parser_interface.hpp>
namespace avs
{
	Packetizer::Packetizer()
		: PipelineNode(new Packetizer::Private(this))
	{
		setNumInputSlots(1);
	}


	void Packetizer::flush()
	{
		if (!d().m_parser->flush())
		{
			AVSLOG(Warning) << "Packetizer: Error while flushing stream parser";
		}

		d().m_buffer.clear();
		d().m_buffer.shrink_to_fit();
	}

	Result Packetizer::configure(StreamParserInterface* parser,size_t numOutputs, int streamIndex)
	{
		d().m_parser.reset(parser);
		if (numOutputs == 0)
		{
			return Result::Node_InvalidConfiguration;
		}
		if (d().m_parser)
		{
			if (Result result = d().m_parser->configure(this, Private::onPacketParsed, 0); !result)
			{
				return result;
			}
		}

		setNumOutputSlots(numOutputs);
		d().streamIndex = streamIndex;
		return Result::OK;
	}

	Result Packetizer::deconfigure()
	{
		flush();
		setNumOutputSlots(0);
		return Result::OK;
	}

	Result Packetizer::process(uint64_t timestamp, uint64_t deltaTime)
	{
		if ( getNumOutputSlots() == 0)
		{
			return Result::Node_NotConfigured;
		}
		return d().process(timestamp);
	}

	Result Packetizer::Private::process(uint64_t timestamp)
	{
		if (!m_parser)
		{
			return Result::Node_NotConfigured;
		}
		if (m_buffer.size() > 0)
		{
			Result result = m_parser->parse(m_buffer.data(), m_buffer.size());
			if (result)
			{
				result = m_parser->flush();
			}
			m_buffer.clear();
			return result;
		}
		return Result::OK;
	}

	Result Packetizer::read(PipelineNode* reader, void* buffer, size_t& bufferSize, size_t& bytesRead)
	{
		AVSLOG(Warning) << "Attempted to read from Packetizer node";
		return Result::Node_NotSupported;
	}

	Result Packetizer::write(PipelineNode* writer, const void* buffer, size_t bufferSize, size_t& bytesWritten)
	{
		if (bufferSize > 0)
		{
			try
			{
				d().m_buffer.insert(d().m_buffer.end(),
					static_cast<const char*>(buffer),
					static_cast<const char*>(buffer) + bufferSize
				);
			}
			catch (const std::bad_alloc&)
			{
				return Result::IO_OutOfMemory;
			}
		}
		bytesWritten = bufferSize;
		return Result::OK;
	}

	Result Packetizer::Private::onPacketParsed(PipelineNode* node, uint32_t inputNodeIndex, const char* buffer, size_t dataSize, size_t dataOffset, bool isLastPayload)
	{
		assert(node);
		Packetizer* self = static_cast<Packetizer*>(node);
		for (int i = 0; i < (int)self->getNumOutputSlots(); ++i)
		{
			PacketInterface* outputNode = dynamic_cast<PacketInterface*>(self->getOutput(i));
			assert(outputNode);
			if (Result result = outputNode->writePacket(self, buffer + dataOffset, dataSize, self->d().streamIndex); result != Result::OK)
			{
				AVSLOG(Error) << "Packetizer: Failed to write to output node: " << i;
				return result;
			}
		}
		return Result::OK;
	}

	Result Packetizer::onOutputLink(int slot, PipelineNode* node)
	{
		if (!dynamic_cast<PacketInterface*>(node))
		{
			AVSLOG(Error) << "Packetizer: Output node does not implement packet IO operations";
			return Result::Node_Incompatible;
		}
		return Result::OK;
	}

} // avs