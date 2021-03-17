// (C) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <crossplatform/MemoryUtil.h>

class PC_MemoryUtil : public scr::MemoryUtil
{
public:
	long getAvailableMemory() const override;
	long getTotalMemory() const override;
	void printMemoryStats() const override;
};
