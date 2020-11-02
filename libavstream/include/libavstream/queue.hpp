// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

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
 * \note Sharing an instance of this node is the recommended way to link two pipelines running on different threads.
 */
	class AVSTREAM_API Queue final : public Node
		, public IOInterface
	{
		AVSTREAM_PUBLICINTERFACE(Queue)
		Queue::Private *data;
	public:
		Queue();

		/*!
		 * Configure queue.
		 * \param maxBuffers Maximum number of buffers in queue (capacity).
		 * \warning Reconfiguring an already configured Queue performs an implicit flush.
		 * \return 
		 *  - Result::OK on success.
		 *  - Result::Node_InvalidConfiguration if maxBuffers is zero.
		 */
		Result configure(size_t maxBuffers,const char *name);

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
		Result read(Node*, void* buffer, size_t& bufferSize, size_t& bytesRead) override;

		/*!
		 * Write buffer to the back of the queue.
		 * \sa IOInterface::write()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::IO_Full if attempted to write to full queue.
		 *  - Result::IO_OutOfMemory if failed to allocate memory for the new queue buffer.
		 */
		Result write(Node*, const void* buffer, size_t bufferSize, size_t& bytesWritten) override;

		/*!
		 * Emplace buffer at the back of the queue.
		 * \sa IOInterface::emplace()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::IO_Full if attempted to write to full queue.
		 *  - Result::IO_OutOfMemory if failed to allocate memory for the queue buffer.
		 */
		Result emplace(Node*, std::vector<char>&& buffer, size_t& bytesWritten);


	
		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "Queue"; }
	};

} // avs