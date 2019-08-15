// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/UniformBuffer.h>
#include <GlBuffer.h>
#include <OVR_GlUtils.h>

namespace scc
{
class GL_UniformBuffer final : public scr::UniformBuffer
	{
	private:
		OVR::GlBuffer m_UBO;

	public:
		GL_UniformBuffer(scr::RenderPlatform *r)
			:scr::UniformBuffer(r) {}

		//Binding Locations for UBOs
		//Camera = 0;
		//Model = 1;
		//Light = 2;
		void Create(UniformBufferCreateInfo* pUniformBufferCreateInfo) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		void Submit() const override;
        bool ResourceInUse(int timeout) override {return true;}
	};
}