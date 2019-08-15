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
			size_t indexCount;
			size_t stride;
			const uint8_t* data;
		};

	protected:
		IndexBufferCreateInfo m_CI;
	
	public:
		IndexBuffer(RenderPlatform *r) :APIObject(r) {}
		virtual ~IndexBuffer()
		{
			m_CI.usage = BufferUsageBit::UNKNOWN_BIT;
			m_CI.indexCount = 0;
			m_CI.stride = 0;
			m_CI.data = nullptr;
		};

		virtual void Create(IndexBufferCreateInfo* pIndexBufferCreateInfo) = 0;
		virtual void Destroy() = 0;

		inline size_t GetIndexCount() const { return m_CI.indexCount; }

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(IndexBuffer*, int)> ResourceInUseCallback = &IndexBuffer::ResourceInUse;

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}