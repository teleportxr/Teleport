// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/UniformBuffer.h>
#include <GlBuffer.h>
#include <OVR_GlUtils.h>

namespace scr
{
class GL_UniformBuffer final : public UniformBuffer
	{
	private:
		OVR::GlBuffer& m_UBO;

	public:
		GL_UniformBuffer(OVR::GlBuffer& ubo);

		//Binding Locations for UBOs
		//Camera = 0;
		//Model = 1;
		//Light = 2;
		void Create(size_t size, const void* data, uint32_t bindingLocation) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		void Submit() const override;
        bool ResourceInUse(int timeout) override {return true;}
	};
}