// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"

namespace clientrender
{


	//Interface for IndexBuffer
	class IndexBuffer :public APIObject
	{
	public:
		struct IndexBufferCreateInfo
		{
			BufferUsageBit usage = BufferUsageBit::UNKNOWN_BIT;
			size_t indexCount = 0;
			size_t stride = 0;
			const uint8_t* data = nullptr;
		};

	protected:
		IndexBufferCreateInfo m_CI;
	
	public:
		IndexBuffer(const RenderPlatform* const r)
			:APIObject(r), m_CI()
		{}

		virtual ~IndexBuffer()
		{
			m_CI.usage = BufferUsageBit::UNKNOWN_BIT;
			m_CI.indexCount = 0;
			m_CI.stride = 0;
			m_CI.data = nullptr;
		};

		virtual void Create(IndexBufferCreateInfo* pIndexBufferCreateInfo) = 0;
		virtual void Destroy() = 0;

		inline const IndexBufferCreateInfo& GetIndexBufferCreateInfo() const { return m_CI; }

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(IndexBuffer*, int)> ResourceInUseCallback = &IndexBuffer::ResourceInUse;

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}