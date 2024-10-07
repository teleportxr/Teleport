// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <vector>

namespace avs
{
/*!
 * Queue node `[passive, 1/1]`
 *
 * A thread-safe, lock-free, producer-consumer queue of byte buffers.
 *
 * Sharing an instance of this node is the recommended way to link two pipelines running on different threads.
 */
	class AVSTREAM_API LockFreeQueue final : public PipelineNode
		, public IOInterface
	{
		AVSTREAM_PUBLICINTERFACE(LockFreeQueue)
		LockFreeQueue::Private *data;
		/** Contiguous ring buffer */
		std::vector<uint8_t> ringBuffer;
		
		std::atomic<size_t> loopback_index = {0};
		std::atomic<size_t> next_write_index = {0};
		std::atomic<size_t> next_read_index = {0};
		std::atomic<size_t> blockCount = {0};
		size_t maxBufferSize=1024*1024*32;
		
		int write_block_count=0;
		int read_block_count=0;
		void flushInternal();
		size_t increaseBufferSize(size_t requestedSize);
		/*!
		 * Flush the queue.
		 */
		void flush();
	public:
		LockFreeQueue();
		~LockFreeQueue();

		// Prevent copying and moving because this class handles raw memory
		LockFreeQueue(const LockFreeQueue&) = delete;
		LockFreeQueue(const LockFreeQueue&&) = delete;

		/*!
		 * Configure queue.
		 * \param maxBufferSize Maximum size of a buffer in the queue.
		 * \param maxBuffers Maximum number of buffers in the queue.
		 * \param name Name of the node.
		 * \warning Reconfiguring an already configured Queue performs an implicit flush.
		 * \return 
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if maxBuffers is zero.
		 */
		Result configure(size_t ringBufferSizeBytes, const char *name);

		/*!
		 * Flush & deconfigure queue.
		 * \return Always returns Result::OK.
		 */
		Result deconfigure() override;
		
		/*!
		 * Read buffer at the front of the queue.
		 * \sa IOInterface::read()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::IO_Empty if attempted to read from empty queue.
		 *  - Result::IO_Retry if buffer is nullptr or bufferSize is smaller than the size of the buffer at the front of queue.
		 *                     Correct buffer size is written back to bufferSize parameter and the read should be retried.
		 */
		Result read(PipelineNode*, void* buffer, size_t& bufferSize, size_t& bytesRead) override;

		void drop() override;
		/*!
		 * Write buffer to the back of the queue.
		 * \sa IOInterface::write()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::IO_Full if attempted to write to full queue.
		 *  - Result::IO_OutOfMemory if failed to allocate memory for the new queue buffer.
		 */
		Result write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten) override;

		size_t bytesRemaining() const;
	
		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return name.c_str(); }
	};

} // avs