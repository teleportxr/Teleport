// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include <ClientRender/VertexBuffer.h>
#include <ClientRender/VertexBufferLayout.h>

namespace platform
{
	namespace crossplatform
	{
		class Buffer;
		class Layout;
	}
}

namespace pc_client
{
	class PC_VertexBuffer final : public clientrender::VertexBuffer
	{
	private:
		platform::crossplatform::Buffer *m_SimulBuffer;
		platform::crossplatform::Layout *m_layout;
	public:
		PC_VertexBuffer(const clientrender::RenderPlatform*const r);

		platform::crossplatform::Layout* GetLayout()
		{
			return m_layout;
		}
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		platform::crossplatform::Buffer *GetSimulVertexBuffer()
		{
			return m_SimulBuffer;
		}

		const platform::crossplatform::Buffer* GetSimulVertexBuffer() const
		{
			return m_SimulBuffer;
		}

		// Inherited via VertexBuffer
		void Create(VertexBufferCreateInfo * pVertexBufferCreateInfo) override;
	};
}