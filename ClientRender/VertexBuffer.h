// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "VertexBufferLayout.h"

namespace clientrender
{
	//Interface for VertexBuffer
	class VertexBuffer : public APIObject
	{
	public:
		struct VertexBufferCreateInfo
		{
			std::shared_ptr<VertexBufferLayout> layout;
			BufferUsageBit usage = BufferUsageBit::UNKNOWN_BIT;
			size_t vertexCount = 0;
			size_t size = 0;
			const void* data = nullptr;
		};

    protected:
		VertexBufferCreateInfo m_CI;
	
	public:
		VertexBuffer(const RenderPlatform* const r)
			:APIObject(r), m_CI()
		{}

		virtual ~VertexBuffer()
		{
			m_CI.layout = nullptr;
			m_CI.usage = BufferUsageBit::UNKNOWN_BIT;
			m_CI.vertexCount = 0;
			m_CI.size = 0;
			m_CI.data = nullptr;
		};


		virtual void Create(VertexBufferCreateInfo* pVertexBufferCreateInfo) = 0;
		virtual void Destroy() = 0;

		inline size_t GetVertexCount() const { return m_CI.vertexCount; }

		virtual bool ResourceInUse(int timeout)= 0;
		std::function<bool(VertexBuffer*, int)> ResourceInUseCallback = &VertexBuffer::ResourceInUse;

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}