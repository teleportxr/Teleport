// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>

namespace avs
{

/*!
 * Null sink node `[passive, N/0]`
 *
 * Silently discards all data written to it.
 */
class AVSTREAM_API NullSink final : public Node
	                              , public IOInterface
	                              , public PacketInterface
{
	AVSTREAM_PUBLICINTERFACE(NullSink)
public:
	NullSink();
	
	/*!
	 * Configure null sink node.
	 * \param numInputs Number of input slots.
	 * \return Always returns Result::OK.
	 */
	Result configure(size_t numInputs);

	/*!
	 * Deconfigure null sink node.
	 * \return Always returns Result::OK.
	 */
	Result deconfigure() override;

	/*!
	 * Null sink node does not support read operations.
	 * \return Always returns Result::Node_NotSupported.
	 */
	Result read(Node* reader, void* buffer, size_t& bufferSize, size_t& bytesRead) override;

	/*!
	 * Does nothing.
	 * \return Always returns Result::OK.
	 */
	Result write(Node* writer, const void* buffer, size_t bufferSize, size_t& bytesWritten) override;

	/*!
	 * Null sink node does not support read operations.
	 * \return Always returns Result::Node_NotSupported.
	 */
	Result readPacket(Node* reader, void* buffer, size_t& bufferSize, int streamIndex) override;

	/*!
	 * Does nothing.
	 * \return Always returns Result::OK.
	 */
	Result writePacket(Node* writer, const void* buffer, size_t bufferSize, int streamIndex) override;
	
	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "NullSink"; }
};

} // avs