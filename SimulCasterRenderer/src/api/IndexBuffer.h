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
		virtual ~IndexBuffer()
		{
			m_Size = 0;
			m_Data = nullptr;

			m_Count = 0;
		};

		virtual void Create(size_t size, const uint32_t* data) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		inline size_t GetCount() const { return m_Count; }

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(IndexBuffer*, int)> ResourceInUseCallback = &IndexBuffer::ResourceInUse;
	};
}