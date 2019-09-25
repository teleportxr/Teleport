// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/VertexBuffer.h>
#include <api/VertexBufferLayout.h>

namespace simul
{
	namespace crossplatform
	{
		class Buffer;
		class Layout;
	}
}

namespace pc_client
{
	class PC_VertexBuffer final : public scr::VertexBuffer
	{
	private:
		simul::crossplatform::Buffer *m_SimulBuffer;
		simul::crossplatform::Layout *m_layout;
	public:
		PC_VertexBuffer(scr::RenderPlatform* r);

		simul::crossplatform::Layout* GetLayout()
		{
			return m_layout;
		}
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		simul::crossplatform::Buffer *GetSimulVertexBuffer()
		{
			return m_SimulBuffer;
		}

		const simul::crossplatform::Buffer* GetSimulVertexBuffer() const
		{
			return m_SimulBuffer;
		}

		// Inherited via VertexBuffer
		void Create(VertexBufferCreateInfo * pVertexBufferCreateInfo) override;
	};
}