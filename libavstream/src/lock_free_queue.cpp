// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#include "lock_free_queue.h"
#include <algorithm>
#include <iostream>
#include "common_p.hpp"
#include "node_p.hpp"
#include "logger.hpp"
#include <libavstream/queue.hpp>
#pragma optimize("",off)
static int write_block_count=0;
static int read_block_count=0;
avs::Logger logger(avs::LogSeverity::Info);
#define QUEUE_LOGGING 0
#if QUEUE_LOGGING
#define SUMMARIZE(event,block,count,rd,wr,ct,rd_blk,wr_blk) AVS_LOG("{0} {1}({2}) - next_read_index: {3}, next_write_index: {4}, blockCount: {5}={6}\n\twill read: {7} from {8}\n",#event,block,count,rd,wr,ct,(wr_blk-rd_blk),*((const size_t*)&ringBuffer[next_read_index.load()]),next_read_index.load());
#define QUEUE_LOG(txt,...) AVS_LOG(txt, ##__VA_ARGS__)
#define QUEUE_LOG_SIMPLE(txt,...) AVS_LOG_SIMPLE(txt, ##__VA_ARGS__)
#define QUEUE_WARN(txt,...) AVS_WARN(txt, ##__VA_ARGS__)
#else
#define SUMMARIZE(event,block,count,rd,wr,ct,rd_blk,wr_blk)
#define QUEUE_LOG(txt,...)
#define QUEUE_LOG_SIMPLE(txt,...)
#define QUEUE_WARN(txt,...)
#endif
#define INDEX_SIZE (sizeof(size_t))
#define ROUNDUP_TO_INDEX_SIZE(a) (((a)+INDEX_SIZE-1)-(((a)+INDEX_SIZE-1)%INDEX_SIZE))
#define ROUND_DOWN_TO_INDEX_SIZE(a) ((a)-((a)%INDEX_SIZE))


namespace avs
{
	struct LockFreeQueue::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(LockFreeQueue, PipelineNode)
	};
} // avs

#define SAFE_DELETE_ARRAY(p)\
	{\
		if(p)\
		{\
			delete[] p;\
			(p)=nullptr;\
		}\
	}
	
using namespace avs;
LockFreeQueue::LockFreeQueue()
	: PipelineNode(new LockFreeQueue::Private(this))
{
	setNumSlots(1, 1);
	data = (LockFreeQueue::Private*)this->m_d;
}

LockFreeQueue::~LockFreeQueue()
{
	flushInternal();
}

Result LockFreeQueue::configure(size_t ringBufferSize, const char *n)
{
	if (ringBufferSize == 0)
	{
		return Result::Node_InvalidConfiguration;
	}
	name=n;
	flushInternal();
	blockCount=0;
	next_read_index = 0;
	next_write_index = 0;
	ringBuffer.resize(ringBufferSize);
	return Result::OK;
}

Result LockFreeQueue::deconfigure()
{
	flushInternal();
	blockCount=0;
	next_read_index = 0;
	next_write_index = 0;
	ringBuffer.clear();
	return Result::OK;
}

void LockFreeQueue::flush()
{
	flushInternal();
}

Result LockFreeQueue::read(PipelineNode*, void* buffer, size_t& bufferSize, size_t& bytesRead)
{
	bytesRead = 0;
	// we only want to read if there's something to read. It's fine for this compare to produce a false result,
	// even if next_write_index changes immediately. That means there's still data to read.
	if(blockCount==0)
	{
		bufferSize=0;
		return Result::IO_Empty;
	}

	const size_t *read_from=(const size_t*)&ringBuffer[next_read_index];
	size_t frontSize=*read_from;
	if(frontSize==0)
	{
		bufferSize = 0;
		return Result::IO_Empty;
	}
	if(frontSize>=ringBuffer.size())
	{
		bufferSize=0;
		AVS_WARN("Want to read: {0} from {1}. Failed to read usable bufferSize from Ringbuffer.\n",frontSize,next_read_index.load());
		return Result::Failed;
	}
	if (!buffer || bufferSize < frontSize)
	{
		bufferSize = frontSize;
		return Result::IO_Retry;
	}
	QUEUE_LOG_SIMPLE("This read: {0} from {1}\n",frontSize,next_read_index.load());
	bytesRead = frontSize;
	auto nxtread=next_read_index.load();
	// where would  we write the next index?
	nxtread	=ROUNDUP_TO_INDEX_SIZE(nxtread+INDEX_SIZE+frontSize);
	read_from++;
	const void* front= (uint8_t*)read_from;
	// if the whole buffer AND the next index don't fit in ringBuffer, we will loop back to zero.
	if(nxtread+INDEX_SIZE>ringBuffer.size())
	{
		front=ringBuffer.data();
		nxtread=ROUNDUP_TO_INDEX_SIZE(frontSize);
	}
	
	/*if(next_read_index+INDEX_SIZE+frontSize>=ringBuffer.size())
	{
		front=ringBuffer.data();
		nxtread=ROUNDUP_TO_INDEX_SIZE(bytesRead);
		QUEUE_LOG_SIMPLE("nxtread: {0}",nxtread);
	}*/

	memcpy(buffer, front, frontSize);
	bytesRead = frontSize;
	SUMMARIZE(pop,read_block_count++,bytesRead,nxtread,next_write_index.load(),blockCount.load(),read_block_count+1,write_block_count)
	size_t rd=*((uint64_t*)&ringBuffer[nxtread]);
	QUEUE_LOG_SIMPLE("Next read: {0} from {1}\n",rd,nxtread);
	if(blockCount>1&&rd>ringBuffer.size()){
		QUEUE_WARN("Next read: {0} from {1}\n",rd,nxtread);
	}
	next_read_index.store(nxtread);
	
	blockCount--;

	return Result::OK;
}

void LockFreeQueue::drop()
{
	blockCount = 0;
	next_write_index = 0;
	next_read_index = 0;
}

Result LockFreeQueue::write(PipelineNode*, const void* buffer, size_t bufferSize, size_t& bytesWritten)
{
	if (!ringBuffer.size())
	{
		QUEUE_WARN("LockFreeQueue::write error: ringbuffer size is zero, unconfigured?\n");
		return Result::Failed;
	}
	if (!ringBuffer.size())
	{
		QUEUE_WARN("LockFreeQueue [0} error: unconfigured?\n",name);
		bytesWritten=0;
		return Result::Failed;
	}
	if (blockCount == 0)
	{
		QUEUE_LOG("Queue is empty, starting from 0.\n");
		next_write_index = 0;
		loopback_index=0;
		next_read_index=0;
	}
	size_t size_write_index=next_write_index;
	// First write the bufferSize. We should always have space to do this.
	memcpy(&ringBuffer[size_write_index], &bufferSize, INDEX_SIZE);
	size_t data_write_index = (size_write_index + INDEX_SIZE) % ringBuffer.size();
	// If it overflows the ring buffer, and read has already looped, we should go back to the start.
	size_t read_block=next_read_index.load();
	size_t spaceRemaining;
	size_t nxtwrite;
	int test=0;
	size_t bufferSpace8=ROUNDUP_TO_INDEX_SIZE(bufferSize);	// reading from the front. We can loop around.
	if(read_block<data_write_index)
	{
		test|=1;
		spaceRemaining=ROUND_DOWN_TO_INDEX_SIZE(ringBuffer.size()-data_write_index);
		// Must have room for the buffer to write.
		if(spaceRemaining<=bufferSpace8)
		{
			test|=2;
			if(read_block>bufferSpace8)
			{
				test|=4;
				loopback_index=data_write_index;
				data_write_index=0;
				spaceRemaining=read_block;
			}
			else
			{
				test|=8;
			// out of space. Must resize.
				size_write_index=increaseBufferSize(bufferSpace8);
				data_write_index=size_write_index+INDEX_SIZE;
				read_block=next_read_index.load();
			}
		}
		//2
		nxtwrite=ROUNDUP_TO_INDEX_SIZE(data_write_index+bufferSize);
		// if the whole buffer AND the next index don't fit in ringBuffer, we will loop back to zero.
		if(nxtwrite+INDEX_SIZE>ringBuffer.size())
		{
			if(bufferSize>read_block){
				QUEUE_WARN("Can't write here. {0}\n",test);
			}
			nxtwrite=0;
		}
		//2
	}
	else // Reading from the end. We have just the space between data_write_index and read_block
	{
		spaceRemaining=read_block-data_write_index;
		if(spaceRemaining<=bufferSize){
		// out of space. Must resize.
			size_write_index=increaseBufferSize(bufferSize);
			data_write_index=size_write_index+INDEX_SIZE;
			read_block=next_read_index.load();
		}
		//2
		nxtwrite=ROUNDUP_TO_INDEX_SIZE(data_write_index+bufferSize);
		// if the whole buffer AND the next index don't fit in ringBuffer, we will loop back to zero.
		if(nxtwrite+INDEX_SIZE>ringBuffer.size())
		{
			nxtwrite=0;
			if(bufferSize>read_block){
				QUEUE_WARN("Can't write here.\n");
			}
		}
		//2
	}
	uint8_t *target=&ringBuffer[data_write_index];
	memcpy(target,buffer,bufferSize);
	SUMMARIZE(push,write_block_count++,bufferSize,next_read_index.load(),nxtwrite,blockCount.load(),read_block_count,write_block_count+1)
	next_write_index.store(nxtwrite);
	blockCount++;
	bytesWritten = bufferSize;
	return Result::OK;
}

void LockFreeQueue::flushInternal()
{
	ringBuffer.clear();
	next_read_index=0;
	next_write_index=0;
	blockCount=0;
}

size_t LockFreeQueue::increaseBufferSize(size_t requestedSize)
{
	const size_t oldBufferSize = ringBuffer.size();
	if(oldBufferSize>=maxBufferSize)
		return 0;
	// For the duration of this function, prevent any new reads
	size_t old_blockCount=std::atomic_exchange(&blockCount,0);
	size_t old_read_index=std::atomic_exchange(&next_read_index,next_write_index);
	size_t max_copy_index=next_write_index+INDEX_SIZE;
	size_t newSize= oldBufferSize+ requestedSize*12;
	if(newSize>maxBufferSize)
		newSize=maxBufferSize;
	std::vector<uint8_t> new_mem(newSize);

	// copy the data to the new buffer. 
	// if Write>Read:
	// |-----|Read|++++++++++++++|Write|-----|
	// |Read|++++++++++++++|Write|-------------------------------|
	// but if Write<Read:
	// |+++++|Write|----------|Read|+++++++++|
	// |Read|++++++++++++++|Write|-------------------------------|
	size_t totalSize=0;
	int64_t size1,size2;
	if (old_blockCount)
	{
	// data from the current next_read_index to the max_copy_index (or end of buffer, write<read):
	// let size1 be the data to be copied from read, going upwards.
		size1=((max_copy_index<old_read_index)?loopback_index.load():max_copy_index)-old_read_index;
	// If necessary, let size2 be the data to be copied from the start up to Write.
		size2=(max_copy_index<old_read_index)?max_copy_index:0;
		totalSize=size1+size2;
		if(totalSize>newSize)
			newSize=totalSize;
		if(size1>0)
			memcpy(new_mem.data(),&(ringBuffer[old_read_index]),size1);
		uint8_t *target=&(new_mem[size1]);
		if(size2>0)
			memcpy(target,ringBuffer.data(),(size_t)size2);
		// end of the new buffer.
		ringBuffer=std::move(new_mem);
	}

	
	//next_read_index should be zero.
	// next_write_index will be totalSize
	// blockCount unchanged.
	//size_t old_read_index=std::atomic_exchange(&next_read_index,next_write_index);
	next_read_index=0;
	// Because totalSize includes the index that we're writing, subtract INDEX_SIZE.
	size_t nxtwrite	=ROUNDUP_TO_INDEX_SIZE(totalSize)-INDEX_SIZE;
	next_write_index=nxtwrite;
	//
	#ifdef _DEBUG
	// Check all the indices.
	size_t i=next_read_index;
	while(i<nxtwrite)
	{
		uint64_t *ptr=(uint64_t *)&(ringBuffer[i]);
		size_t sz=*ptr;
		size_t step=sz+INDEX_SIZE+(INDEX_SIZE-1);
		step-=step%INDEX_SIZE;
		if(step>next_write_index-i&&next_write_index-i>INDEX_SIZE){
			QUEUE_WARN("Error");
		}
		i+=step;
	}
	uint64_t *indexptr=(uint64_t *)&(ringBuffer[max_copy_index-INDEX_SIZE]);
	QUEUE_LOG("LockFreeQueue {0} increaseBufferSize to {1} with last value: {2} at {3}, next write is: {4}.\n",name,newSize,*indexptr,max_copy_index,next_write_index.load());
	#endif
	// re-enable reading:
	std::atomic_exchange(&blockCount,old_blockCount);
	return next_write_index;
}



size_t LockFreeQueue::bytesRemaining() const
{
	size_t small=next_read_index;
	size_t large=next_write_index;
	if(large<small)
		large+=ringBuffer.size();
	return large-small;
}
