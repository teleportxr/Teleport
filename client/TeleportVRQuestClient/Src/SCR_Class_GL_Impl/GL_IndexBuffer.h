// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/IndexBuffer.h>
#include <Render/GlGeometry.h>

namespace scc
{
class GL_IndexBuffer final : public clientrender::IndexBuffer
	{
	private:
		GLuint m_IndexID;

	public:
		GL_IndexBuffer(const clientrender::RenderPlatform*const r)
			:clientrender::IndexBuffer(r) {}

		void Create(IndexBufferCreateInfo* pIndexBufferCreateInfo) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		inline const GLuint& GetIndexID() const { return m_IndexID; }
	};
}