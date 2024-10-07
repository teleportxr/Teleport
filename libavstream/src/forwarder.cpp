// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include "forwarder_p.hpp"
#include <algorithm>

namespace avs {

	Forwarder::Forwarder()
		: PipelineNode(new Forwarder::Private(this))
	{}

	Result Forwarder::configure(size_t numInputs, size_t numOutputs, size_t chunkSize)
	{
		if (numInputs == 0 || numOutputs == 0 || chunkSize == 0)
		{
			return Result::Node_InvalidConfiguration;
		}
		if (numInputs > numOutputs)
		{
			return Result::Node_InvalidConfiguration;
		}
		if (numOutputs > numInputs && numInputs > 1)
		{
			return Result::Node_InvalidConfiguration;
		}

		setNumSlots(numInputs, numOutputs);
		try
		{
			d().m_buffer.resize(chunkSize);
			d().m_chunkSize = chunkSize;
		}
		catch (const std::bad_alloc&)
		{
			return Result::IO_OutOfMemory;
		}
		return Result::OK;
	}

	Result Forwarder::deconfigure()
	{
		setNumSlots(0, 0);
		d().m_buffer.clear();
		d().m_buffer.shrink_to_fit();
		d().m_chunkSize = 0;
		return Result::OK;
	}

	Result Forwarder::process(uint64_t timestamp, uint64_t deltaTime)
	{
		const size_t numInputs = getNumInputSlots();
		const size_t numOutputs = getNumOutputSlots();
		if (numInputs == 0 || numOutputs == 0)
		{
			return Result::Node_NotConfigured;
		}

		auto readInput = [this](PipelineNode* node, size_t& numBytesRead) -> Result
		{
			assert(node);
			assert(d().m_buffer.size() >= d().m_chunkSize);

			if (IOInterface* nodeIO = dynamic_cast<IOInterface*>(node))
			{
				size_t bufferSize = d().m_chunkSize;
				Result result = nodeIO->read(this, d().m_buffer.data(), bufferSize, numBytesRead);
				if (result == Result::IO_Retry)
				{
					d().m_buffer.resize(bufferSize);
					result = nodeIO->read(this, d().m_buffer.data(), bufferSize, numBytesRead);
				}
				numBytesRead = std::min(bufferSize,numBytesRead);
				return result;
			}
			else
			{
				assert(false);
				return Result::Node_Incompatible;
			}
		};

		auto writeOutput = [this](PipelineNode* node, size_t numBytesToWrite) -> Result
		{
			assert(node);
			assert(d().m_buffer.size() >= numBytesToWrite);

			if (IOInterface* nodeIO = dynamic_cast<IOInterface*>(node))
			{
				size_t bytesWritten;
				return nodeIO->write(this, d().m_buffer.data(), numBytesToWrite, bytesWritten);
			}
			else
			{
				assert(false);
				return Result::Node_Incompatible;
			}
		};
		try
		{
			if (numOutputs == numInputs)
			{
				for (int i = 0; i < (int)getNumInputSlots(); ++i)
				{
					size_t numBytesRead;
					if (Result result = readInput(getInput(i), numBytesRead); result != Result::OK)
					{
						if (result != Result::IO_Empty)
						{
							AVSLOG(Error) << "Forwarder: Failed to read from input node: " << i;
						}
						return result;
					}
					if (Result result = writeOutput(getOutput(i), numBytesRead); result != Result::OK)
					{
						if (result != Result::IO_Full)
						{
							AVSLOG(Error) << "Forwarder: Failed to write to output node: " << i;
						}
						return result;
					}
				}
			}
			else
			{
				size_t numBytesRead;
				if (Result result = readInput(getInput(0), numBytesRead); result != Result::OK)
				{
					if (result != Result::IO_Empty)
					{
						AVSLOG(Error) << "Forwarder: Failed to read from input node: 0";
					}
					return result;
				}

				for (int i = 0; i < (int)getNumOutputSlots(); ++i)
				{
					if (Result result = writeOutput(getOutput(i), numBytesRead); result != Result::OK)
					{
						if (result != Result::IO_Full)
						{
							AVSLOG(Error) << "Forwarder: Failed to write to output node: " << i;
						}
						return result;
					}
				}
			}
		}
		catch (const std::bad_alloc&)
		{
			return Result::IO_OutOfMemory;
		}
		return Result::OK;
	}

	Result Forwarder::onInputLink(int slot, PipelineNode* node)
	{
		if (!(dynamic_cast<PacketInterface*>(node) || dynamic_cast<IOInterface*>(node)))
		{
			AVSLOG(Error) << "Forwarder: Input node does not implement IO or packet operations";
			return Result::Node_Incompatible;
		}
		return Result::OK;
	}

	Result Forwarder::onOutputLink(int slot, PipelineNode* node)
	{
		if (!(dynamic_cast<PacketInterface*>(node) || dynamic_cast<IOInterface*>(node)))
		{
			AVSLOG(Error) << "Forwarder: Output node does not implement IO or packet operations";
			return Result::Node_Incompatible;
		}
		return Result::OK;
	}

} // avs