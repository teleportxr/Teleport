// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>

namespace avs
{

/*!
 * Forwarder node `[input-active, output-active, N/M]`
 *
 * Forwarder node reads data from its inputs and passes it to its outputs without modification.
 * It is most useful as a proxy node if one wishes to link two passive nodes within a pipeline.
 * - Compatible inputs: Any node implementing either PacketInterface or IOInterface.
 * - Compatible outputs: Any node implementing either PacketInterface or IOInterface.
 *
 * If an input or an output of a Forwarder node implements both PacketInterface and IOInterface
 *       then PacketInterface is used by the forwarder node to read or write data.
 */
class AVSTREAM_API Forwarder final : public PipelineNode
{
	AVSTREAM_PUBLICINTERFACE(Forwarder)
public:
	Forwarder();

	/*!
	 * Configure forwarder.
	 * Forwarder node has two modes of operation:
	 * - When numInputs is equal to numOutputs (N == M) data from i-th input is forwarded to i-th output (for 0 <= i < N).
	 * - When numInputs is 1 and numOutputs > 1 (N == 1, M > 1) data from input slot 0 is forwarded (broadcasted) to all outputs.
	 * - Any other combination of numInputs and numOutputs is considered invalid.
	 * 
	 * \param numInputs Number of input slots.
	 * \param numOutputs Number of output slots.
	 * \param chunkSize Number of bytes to forward in one process() call when dealing with nodes implementing only IOInterface.
	 *                  Also controls initial buffer size for all reads and writes.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_InvalidConfiguration if chunkSize is zero.
	 *  - Result::Node_InvalidConfiguration if the combination of numInputs and numOutputs is invalid.
	 *  - Result::IO_OutOfMemory if failed to allocate memory for the internal buffer.
	 */
	Result configure(size_t numInputs, size_t numOutputs, size_t chunkSize);

	/*!
	 * Deconfigure forwarder and free the internal buffer.
	 * \return Always returns Result::OK.
	 */
	Result deconfigure() override;

	/*!
	 * Forward data from input nodes to output nodes.
	 * \sa PipelineNode::process()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::IO_OutOfMemory if failed to allocate memory for the internal buffer.
	 *  - Any error result returned by IOInterface::read() and IOInterface::write().
	 *  - Any error result returned by PacketInterface::readPacket() and PacketInterface::writePacket().
	 */
	Result process(uint64_t timestamp, uint64_t deltaTime) override;
	
	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "Forwarder"; }

private:
	Result onInputLink(int slot, PipelineNode* node) override;
	Result onOutputLink(int slot, PipelineNode* node) override;
};

} // avs