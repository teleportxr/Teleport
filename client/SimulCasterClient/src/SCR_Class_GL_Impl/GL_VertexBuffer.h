// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/VertexBuffer.h>
#include <api/VertexBufferLayout.h>
#include <GlGeometry.h>

namespace scr
{
	class GL_VertexBuffer final : public VertexBuffer
	{
	private:
		OVR::GlGeometry m_Geometry;

	public:
        GL_VertexBuffer() {}

		void Create(size_t size, const void* data) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		//Assume an interleaved VBO;
		void CreateVAO(const VertexBufferLayout* layout);

	private:
		void CalculateCount();
	};
}