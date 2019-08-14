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

		void Create(size_t numIndices, size_t stride, const unsigned char* data) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}
	};
}