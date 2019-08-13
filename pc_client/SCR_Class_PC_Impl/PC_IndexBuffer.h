// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/IndexBuffer.h>

namespace pc_client
{
	class PC_IndexBuffer  : public scr::IndexBuffer
	{
	private:

	public:
		PC_IndexBuffer() {}

		void Create(size_t size, const uint32_t* data) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}
	};
}