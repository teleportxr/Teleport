// (C) Copyright 2018-2021 Simul Software Ltd

#pragma once

namespace teleport
{
	namespace clientrender
	{
		class MemoryUtil
		{
		public:
			virtual bool isSufficientMemory(long minimum) const;
			virtual long getAvailableMemory() const = 0;
			virtual long getTotalMemory() const = 0;
			virtual void printMemoryStats() const = 0;

			static const MemoryUtil *Get();

		protected:
			MemoryUtil();
			virtual ~MemoryUtil();

			static const MemoryUtil *mMemoryUtil;
		};
	}
}