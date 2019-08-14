// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

namespace scr
{


	//Interface for IndexBuffer
	class IndexBuffer :public APIObject
	{
	public:
		struct IndexBufferCreateInfo
		{
			BufferUsageBit usage;
		};

	protected:
		IndexBufferCreateInfo m_CI;
		
		size_t m_Size;
		const uint8_t* m_Data;
	
		size_t m_IndexCount = 0;
	
	public:
		IndexBuffer(RenderPlatform *r) :APIObject(r) {}
		virtual ~IndexBuffer()
		{
			m_Data = nullptr;

			m_IndexCount = 0;

			m_CI.usage = BufferUsageBit::UNKNOWN_BIT;
		};

		virtual void Create(IndexBufferCreateInfo* pIndexBufferCreateInfo, size_t numIndices, size_t stride, const uint8_t* data) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		inline size_t GetIndexCount() const { return m_IndexCount; }

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(IndexBuffer*, int)> ResourceInUseCallback = &IndexBuffer::ResourceInUse;
	};
}