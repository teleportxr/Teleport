// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/stream/parser_interface.hpp>

namespace avs
{
/*!
 * Bitstream packetizer node `[output-active, 1/M]`
 *
 * Accepts bitstream as input and broadcasts discrete packets to all its outputs.
 * Expected bitstream format and output packets payload is defined by the selected stream parser.
 *
 * \note When used with AVC_AnnexB parser it accepts AVC Annex B bitstream input and outputs individual NAL units.
 *
 * - Compatible outputs: Any node implementing PacketInterface.
 */
class AVSTREAM_API Packetizer final : public PipelineNode
	                                , public IOInterface
{
	AVSTREAM_PUBLICINTERFACE(Packetizer)
public:
	/*!
	 * Constructor.
	 * \param parser Bitstream parser type to use.
	 */
	explicit Packetizer();


	/*! Flush the parser and free any internal resources. */
	void flush();

	/*!
	 * Configure packetizer.
	 * \note Packetizer node takes ownerhip of the parser instance.
	 * \param numOutputs Number of output slots.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_InvalidConfiguration if numOutputs is zero.
	 */
	Result configure(StreamParserInterface* parser,size_t numOutputs, int streamIndex);

	/*!
	 * Deconfigure packetizer.
	 * \note This function performs an implicit flush.
	 * \return Always returns Result::OK.
	 */
	Result deconfigure() override;

	/*!
	 * Parse bitstream and broadcast packets to all outputs.
	 * \sa PipelineNode::process()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if packetizer has not been configured.
	 *  - Any error result returned by StreamParserInterface::parse().
	 */
	Result process(uint64_t timestamp, uint64_t deltaTime) override;

	/*!
	 * Packetizer node does not support read operations.
	 * \return Always returns Result::Node_NotSupported.
	 */
	Result read(PipelineNode* reader, void* buffer, size_t& bufferSize, size_t& bytesRead) override;

	/*!
	 * Write bitstream to packetizer.
	 * \sa IOInterface::write()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::IO_OutOfMemory if failed to allocate memory for internal buffer.
	 */
	Result write(PipelineNode* writer, const void* buffer, size_t bufferSize, size_t& bytesWritten) override;
	
	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "Packetizer"; }

private:
	Result onOutputLink(int slot, PipelineNode* node) override;
};

} // avs