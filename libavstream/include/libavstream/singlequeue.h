// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <vector>

namespace avs
{
/*!
 * Queue node `[passive, 1/1]`
 *
 * A thread-safe, nonblocking, pseudo-queue of either one, or zero byte buffers. Pushing a buffer replaces the
 * one that's there, if any.
 * Sharing an instance of this node is a way to link two pipelines running on different threads.
 */
	class AVSTREAM_API SingleQueue final : public PipelineNode
		, public IOInterface
	{
		AVSTREAM_PUBLICINTERFACE(SingleQueue)
		SingleQueue::Private *data;
		std::vector<uint8_t> buffer;
		size_t dataSize = 0;
		std::mutex m_mutex;
		void flushInternal();
		void increaseBufferSize(size_t requestedSize);
		void push(const void* buffer, size_t bufferSize);
		void pop();
		void drop();
	public:
		SingleQueue();

		~SingleQueue();

		// Prevent copying and moving because this class handles raw memory
		SingleQueue(const SingleQueue&) = delete;

		SingleQueue(const SingleQueue&&) = delete;

		/*!
		 * Configure SingleQueue.
		 * \param maxBufferSize Maximum size of a buffer in the queue.
		 * \param name Name of this node.
		 * \warning Reconfiguring an already configured Queue performs an implicit flush.
		 * \return 
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if maxBuffers is zero.
		 */
		Result configure(size_t maxBufferSize, const char *name);

		/*!
		 * Flush & deconfigure queue.
		 * \return Always returns Result::OK.
		 */
		Result deconfigure() override;

		/*!
		 * Flush queue.
		 */
		void flush();

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

		/*!
		 * Write buffer to the queue. Replaces any data that is already there. Only one buffer is ever present.
		 * \sa IOInterface::write()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::IO_Full if attempted to write to full queue.
		 *  - Result::IO_OutOfMemory if failed to allocate memory for the new queue buffer.
		 */
		Result write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten) override;

	
		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return name.c_str(); }
	};

} // avs