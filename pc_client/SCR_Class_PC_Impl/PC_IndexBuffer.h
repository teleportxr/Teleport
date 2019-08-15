// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/IndexBuffer.h>

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
		PC_IndexBuffer(scr::RenderPlatform *r):scr::IndexBuffer(r) {}

		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		// Inherited via IndexBuffer
		void Create(IndexBufferCreateInfo * pIndexBufferCreateInfo) override;
	};
}