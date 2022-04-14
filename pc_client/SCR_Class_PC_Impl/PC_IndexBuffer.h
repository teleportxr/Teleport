// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include <ClientRender/IndexBuffer.h>

namespace platform
{
	namespace crossplatform
	{
		class Buffer;
	}
}

namespace pc_client
{
	class PC_IndexBuffer  : public clientrender::IndexBuffer
	{
	private:
		platform::crossplatform::Buffer *m_SimulBuffer;
	public:
		PC_IndexBuffer(const clientrender::RenderPlatform*const r)
			:clientrender::IndexBuffer(r), m_SimulBuffer(nullptr)
		{}

		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		platform::crossplatform::Buffer* GetSimulIndexBuffer()
		{
			return m_SimulBuffer;
		}
		const platform::crossplatform::Buffer* GetSimulIndexBuffer() const
		{
			return m_SimulBuffer;
		}

		// Inherited via IndexBuffer
		void Create(IndexBufferCreateInfo * pIndexBufferCreateInfo) override;
	};
}