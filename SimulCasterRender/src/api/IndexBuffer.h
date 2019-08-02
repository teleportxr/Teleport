// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"

namespace scr
{
	//Interface for IndexBuffer
	class IndexBuffer
	{
	protected:
		size_t m_Size;
		const uint32_t* m_Data;

		size_t m_Count;

	public:
		IndexBuffer(size_t size, const uint32_t* data)
			:m_Size(size), m_Data(data)
		{
			assert(m_Size % sizeof(uint32_t) == 0);
			m_Count = size / sizeof(uint32_t);
		};

		virtual ~IndexBuffer()
		{
			m_Size = 0;
			m_Data = nullptr;

			m_Count = 0;
		};

		virtual void Create() = 0;
		virtual void Destroy() = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		inline size_t GetCount() const { return m_Count; }
	};
}