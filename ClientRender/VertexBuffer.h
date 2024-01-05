// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "VertexBufferLayout.h"

namespace platform
{
	namespace crossplatform
	{
		class Buffer;
		class Layout;
	}
}

namespace teleport
{
	namespace clientrender
	{
		// Interface for VertexBuffer
		class VertexBuffer : public APIObject
		{
		public:
			struct VertexBufferCreateInfo
			{
				std::shared_ptr<VertexBufferLayout> layout;
				BufferUsageBit usage = BufferUsageBit::UNKNOWN_BIT;
				size_t vertexCount = 0;
				size_t size = 0;
				std::shared_ptr<std::vector<uint8_t>> data;
			};
			std::string getName() const
			{
				return "VertexBuffer";
			}

		protected:
			VertexBufferCreateInfo m_CI;

			platform::crossplatform::Buffer *m_SimulBuffer = nullptr;
			platform::crossplatform::Layout *m_layout = nullptr;

		public:
			VertexBuffer(platform::crossplatform::RenderPlatform *r)
				: APIObject(r), m_CI()
			{
			}

			virtual ~VertexBuffer()
			{
				m_CI.layout = nullptr;
				m_CI.usage = BufferUsageBit::UNKNOWN_BIT;
				m_CI.vertexCount = 0;
				m_CI.size = 0;
				m_CI.data = nullptr;
			};

			platform::crossplatform::Layout *GetLayout()
			{
				return m_layout;
			}

			virtual void Create(VertexBufferCreateInfo *pVertexBufferCreateInfo);
			virtual void Destroy();

			inline size_t GetVertexCount() const { return m_CI.vertexCount; }

			virtual bool ResourceInUse(int timeout) { return true; }
			std::function<bool(VertexBuffer *, int)> ResourceInUseCallback = &VertexBuffer::ResourceInUse;

			platform::crossplatform::Buffer *GetSimulVertexBuffer()
			{
				return m_SimulBuffer;
			}

			const platform::crossplatform::Buffer *GetSimulVertexBuffer() const
			{
				return m_SimulBuffer;
			}

		protected:
			virtual void Bind() const;
			virtual void Unbind() const;
		};
	}
}