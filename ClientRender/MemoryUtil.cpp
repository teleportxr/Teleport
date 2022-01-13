// (C) Copyright 2018-2021 Simul Software Ltd

#include "MemoryUtil.h"
#include <cassert>

namespace clientrender
{
	const MemoryUtil* MemoryUtil::mMemoryUtil = nullptr;

	MemoryUtil::MemoryUtil()
	{
		assert(!mMemoryUtil);
		mMemoryUtil = this;
	}

	MemoryUtil::~MemoryUtil()
	{
		mMemoryUtil = nullptr;
	}

	const MemoryUtil* MemoryUtil::Get()
	{
		return mMemoryUtil;
	}

	bool MemoryUtil::isSufficientMemory(long minimum) const
	{
		long mem = getAvailableMemory();
		return mem >= minimum;
	}
}

