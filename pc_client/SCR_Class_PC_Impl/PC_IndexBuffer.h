// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/IndexBuffer.h>

namespace simul
{
	namespace crossplatform
	{
		class Buffer;
	}
}

namespace pc_client
{
	class PC_IndexBuffer  : public scr::IndexBuffer
	{
	private:
		simul::crossplatform::Buffer *m_SimulBuffer;
	public:
		PC_IndexBuffer(const scr::RenderPlatform*const r)
			:scr::IndexBuffer(r), m_SimulBuffer(nullptr)
		{}

		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		simul::crossplatform::Buffer* GetSimulIndexBuffer()
		{
			return m_SimulBuffer;
		}
		const simul::crossplatform::Buffer* GetSimulIndexBuffer() const
		{
			return m_SimulBuffer;
		}

		// Inherited via IndexBuffer
		void Create(IndexBufferCreateInfo * pIndexBufferCreateInfo) override;
	};
}