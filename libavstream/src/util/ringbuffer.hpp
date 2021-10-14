// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <cstdint>
#include <vector>
#include <optional>

template <class T,size_t max_size>
class RingBuffer
{
public:
	RingBuffer()
	{

	}
	T* reserve_next()
	{
		return buf_ + head_;
	}
	void commit_next()
	{
		if (full_)
		{
			tail_ = (tail_ + 1) % max_size;
		}
		head_ = (head_ + 1) % max_size;

		full_ = head_ == tail_;
	}
	void put(T item)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		T* next = reserve_next();
		*next= item;
		commit_next();
	}
	T get()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if(empty())
		{
			return T();
		}

		//Read data and advance the tail (we now have a free space)
		auto val = buf_[tail_];
		full_ = false;
		tail_ = (tail_ + 1) % max_size;

		return val;
	}

	void copyTail(T* val)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (empty())
		{
			return;
		}

		//Copy data and advance the tail (we now have a free space)
		memcpy(val, &buf_[tail_], sizeof(T));
		full_ = false;
		tail_ = (tail_ + 1) % max_size;
	}

	void reset()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		head_ .store(tail_.load());
		full_ = false;
	}

	bool empty() const
	{
		//if head and tail are equal, we are empty
		return (!full_ && (head_ == tail_));
	}

	bool full() const
	{
		//If tail is ahead the head by 1, we are full
		return full_;
	}

	size_t capacity() const
	{
		return max_size;
	}

	size_t size() const
	{
		size_t sz = max_size;
		if(!full_)
		{
			if(head_ >= tail_)
			{
				sz = head_ - tail_;
			}
			else
			{
				sz = max_size + head_ - tail_;
			}
		}
		return sz;
	}

	private:
	std::mutex mutex_;
	T	buf_[max_size];
	std::atomic_size_t       head_ = 0;
	std::atomic_size_t       tail_ = 0;
	std::atomic_size_t       watermark_ = 0;
	std::atomic_bool         full_ = 0;
};


