// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/VertexBuffer.h>
#include <api/VertexBufferLayout.h>
#include <GlGeometry.h>

namespace scc
{
	class GL_VertexBuffer final : public scr::VertexBuffer
	{
	private:
		GLuint m_VertexID;
		GLuint m_VertexArrayID;

	public:
        GL_VertexBuffer(scr::RenderPlatform* r)
        	:scr::VertexBuffer(r) {}

		void Create(VertexBufferCreateInfo* pVertexBufferCreateInfo) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		bool ResourceInUse(int timeout) override {return true;}

		inline const GLuint& GetVertexID() const { return m_VertexID; }
		inline const GLuint& GetVertexArrayID() const { return m_VertexArrayID; }

	private:
		//Assume an interleaved VBO;
		void CreateVAO();
	};
}