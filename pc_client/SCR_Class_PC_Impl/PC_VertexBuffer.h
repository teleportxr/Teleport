// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/VertexBuffer.h>
#include <api/VertexBufferLayout.h>

namespace simul
{
	namespace crossplatform
	{
		class Buffer;
	}
}

namespace pc_client
{
	class PC_VertexBuffer final : public scr::VertexBuffer
	{
	private:
		simul::crossplatform::Buffer *m_SimulBuffer;
	public:
        PC_VertexBuffer(scr::RenderPlatform *r):scr::VertexBuffer(r) {}

		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		// Inherited via VertexBuffer
		void Create(VertexBufferCreateInfo * pVertexBufferCreateInfo) override;
	};
}