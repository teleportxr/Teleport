// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "VertexBufferLayout.h"

namespace scr
{
	//Interface for VertexBuffer
	class VertexBuffer
	{
	public:
		struct VertexBufferCreateInfo
		{
			std::shared_ptr<VertexBufferLayout> layout;
			BufferUsageBit usage;
		};

	protected:
		VertexBufferCreateInfo m_CI;

		size_t m_Size;
		const void* m_Data;

		size_t m_VertexCount = 0;

	public:
		virtual ~VertexBuffer()
		{
			m_Size = 0;
			m_Data = nullptr;

			m_VertexCount = 0;
			m_CI.layout = nullptr;
			m_CI.usage = BufferUsageBit::UNKNOWN_BIT;
		};


		virtual void Create(VertexBufferCreateInfo* pVertexBufferCreateInfo, size_t size, const void* data) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
		
		inline size_t GetVertexCount() const { return m_VertexCount; }

		virtual bool ResourceInUse(int timeout)= 0;
		std::function<bool(VertexBuffer*, int)> ResourceInUseCallback = &VertexBuffer::ResourceInUse;
	};
}