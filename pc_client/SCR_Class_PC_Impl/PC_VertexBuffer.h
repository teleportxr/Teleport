// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/VertexBuffer.h>
#include <api/VertexBufferLayout.h>

namespace pc_client
{
	class PC_VertexBuffer final : public scr::VertexBuffer
	{
	private:

	public:
        PC_VertexBuffer() {}

		void Create(size_t size, const void* data) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}
	};
}