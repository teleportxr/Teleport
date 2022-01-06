// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>

namespace avs
{

/*!
 * Helper class for enforcing the use of memory allocator defined within libavstream library module.
 *
 * This is used to prevent memory allocation/deallocation across DLL boundary even when operator new
 * or operator delete are called from client code.
 */
class AVSTREAM_API UseInternalAllocator
{
public:
	static void* operator new(size_t size);
	static void  operator delete(void* memory);

protected:
	UseInternalAllocator()
{}
};

} // avs