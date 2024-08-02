// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>

namespace avs
{

/*!
 * Ring buffer node `[passive, 1/1]`
 *
 * A thread-safe, nonblocking, producer-consumer ring buffer with fixed capacity.
 */
class AVSTREAM_API Buffer final : public PipelineNode
	                            , public IOInterface
{
	AVSTREAM_PUBLICINTERFACE(Buffer)
public:
	Buffer();
	
	/*!
	 * Configure buffer.
	 * \param bufferCapacity Capacity of buffer in bytes.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::IO_OutOfMemory if failed to allocate backing memory.
	 */
	Result configure(size_t bufferCapacity);

	/*!
	 * Deconfigure buffer and free backing memory.
	 * \return Always returns Result::OK.
	 */
	Result deconfigure() override;

	/*!
	 * Read bytes from buffer.
	 * \sa IOInterface::read()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if buffer has not yet been configured with capacity.
	 *  - Result::IO_Retry if bufferSize is zero.
	 */
	Result read(PipelineNode*, void* buffer, size_t& bufferSize, size_t& bytesRead) override;

	/*!
	 * Write bytes to buffer.
	 * \sa IOInterface::write()
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if buffer has not yet been configured with capacity.
	 */
	Result write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten) override;
	void drop() override{}
	/*! Get currently configured buffer capacity. */
	size_t getCapacity() const;

	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "Buffer"; }
};

} // avs