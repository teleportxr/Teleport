// (C) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include <ClientRender/MemoryUtil.h>

class PC_MemoryUtil : public teleport::clientrender::MemoryUtil
{
public:
	long getAvailableMemory() const override;
	long getTotalMemory() const override;
	void printMemoryStats() const override;
};
