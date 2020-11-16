// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/ShaderStorageBuffer.h>

namespace pc_client
{
class PC_ShaderStorageBuffer final : public scr::ShaderStorageBuffer
	{
	private:

	public:
		PC_ShaderStorageBuffer(const scr::RenderPlatform* r) :scr::ShaderStorageBuffer(r) {}

		void Create(ShaderStorageBufferCreateInfo * pUniformBuffer) override {}
		void Update(size_t size, const void* data, uint32_t offset = 0) override {}
		void Destroy() override {}

		bool ResourceInUse(int timeout) override { return true; }

		void Bind() const override {}
		void Unbind() const override {}

		void Access() override {}
};
}