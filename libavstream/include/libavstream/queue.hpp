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
 * A thread-safe, nonblocking, producer-consumer queue of byte buffers.
 *
 * Sharing an instance of this node is the recommended way to link two pipelines running on different threads.
 */
	class AVSTREAM_API Queue final : public PipelineNode
		, public IOInterface
	{
		AVSTREAM_PUBLICINTERFACE(Queue)
		Queue::Private *data;
		std::string name;
		/** Contiguous memory that contains buffers of equal size */
		char* m_mem = nullptr;
		/** Contains sizes of data in each buffer */
		size_t* m_dataSizes = nullptr;
		size_t m_originalMaxBufferSize = 0;
		size_t m_originalMaxBuffers = 0;
		size_t m_maxBufferSize = 0;
		size_t m_maxBuffers = 0;
		size_t m_absoluteMaxBuffers = 500000;
		size_t m_numElements = 0;
		int64_t m_front = -1;
		std::mutex m_mutex;
		void flushInternal();
		void increaseBufferCount();
		void increaseBufferSize(size_t requestedSize);
		const void* frontp(size_t& bufferSize) const;
		void push(const void* buffer, size_t bufferSize);
		void pop();
	public:
		Queue();

		~Queue();

		// Prevent copying and moving because this class handles raw memory
		Queue(const Queue&) = delete;

		Queue(const Queue&&) = delete;

		/*!
		 * Configure queue.
		 * \param maxBufferSize Maximum size of a buffer in the queue.
		 * \param maxBuffers Maximum number of buffers in the queue.
		 * \warning Reconfiguring an already configured Queue performs an implicit flush.
		 * \return 
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if maxBuffers is zero.
		 */
		Result configure(size_t maxBufferSize, size_t maxBuffers, const char *name);

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
		 * Write buffer to the back of the queue.
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