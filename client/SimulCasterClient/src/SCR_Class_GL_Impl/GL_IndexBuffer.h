// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/IndexBuffer.h>
#include <GlGeometry.h>

namespace scr
{
	class GL_IndexBuffer final : public IndexBuffer
	{
	private:
		OVR::GlGeometry& m_Geometry;

	public:
		GL_IndexBuffer(OVR::GlGeometry& geometry);

		void Create(size_t size, const uint32_t* data) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}
	};
}