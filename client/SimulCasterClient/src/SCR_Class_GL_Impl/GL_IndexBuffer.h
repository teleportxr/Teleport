// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/IndexBuffer.h>
#include <GlGeometry.h>

namespace scc
{
class GL_IndexBuffer final : public scr::IndexBuffer
	{
	private:
		GLuint m_IndexID;

	public:
		GL_IndexBuffer(const scr::RenderPlatform*const r)
			:scr::IndexBuffer(r) {}

		void Create(IndexBufferCreateInfo* pIndexBufferCreateInfo) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		inline const GLuint& GetIndexID() const { return m_IndexID; }
	};
}