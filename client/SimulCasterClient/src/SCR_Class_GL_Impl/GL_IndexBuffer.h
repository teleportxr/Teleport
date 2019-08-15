// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/IndexBuffer.h>
#include <GlGeometry.h>

namespace scc
{
class GL_IndexBuffer final : public scr::IndexBuffer
	{
	private:
		OVR::GlGeometry m_Geometry;

	public:
		GL_IndexBuffer(scr::RenderPlatform* r)
			:scr::IndexBuffer(r) {}

		void Create(IndexBufferCreateInfo* pIndexBufferCreateInfo) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}
	};
}