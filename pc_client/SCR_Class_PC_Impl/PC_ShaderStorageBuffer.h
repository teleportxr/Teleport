// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/ShaderStorageBuffer.h>

namespace pc_client
{
class PC_ShaderStorageBuffer final : public scr::ShaderStorageBuffer
	{
	private:

	public:
		PC_ShaderStorageBuffer(scr::RenderPlatform *r):scr::ShaderStorageBuffer(r) {}

		void Create(ShaderStorageBufferCreateInfo * pUniformBuffer) override {}
		void Destroy() override {}

		void Bind() const override {}
		void Unbind() const override {}

		void Access() override {}
};
}