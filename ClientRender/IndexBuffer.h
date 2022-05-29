// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"

namespace platform
{
	namespace crossplatform
	{
		class Buffer;
	}
}

namespace clientrender
{
	/// Wrapper for an IndexBuffer
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

		virtual void Create(IndexBufferCreateInfo* pIndexBufferCreateInfo);
		virtual void Destroy();

		inline const IndexBufferCreateInfo& GetIndexBufferCreateInfo() const { return m_CI; }

		virtual bool ResourceInUse(int timeout) {return true;}
		std::function<bool(IndexBuffer*, int)> ResourceInUseCallback = &IndexBuffer::ResourceInUse;
		

		platform::crossplatform::Buffer* GetSimulIndexBuffer()
		{
			return m_SimulBuffer;
		}
		const platform::crossplatform::Buffer* GetSimulIndexBuffer() const
		{
			return m_SimulBuffer;
		}

	protected:
		virtual void Bind() const {}
		virtual void Unbind() const {}
		platform::crossplatform::Buffer *m_SimulBuffer=nullptr;
	};
}