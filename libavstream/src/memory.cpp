// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include <libavstream/memory.hpp>

#include <cstdlib>

namespace avs {

	void* UseInternalAllocator::operator new(size_t size)
	{
		return std::malloc(size);
	}

	void UseInternalAllocator::operator delete(void* memory)
	{
		std::free(memory);
	}

} // avs