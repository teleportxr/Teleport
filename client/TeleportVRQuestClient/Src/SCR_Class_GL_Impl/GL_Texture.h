// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "GL_Sampler.h"
#include <api/Texture.h>
#include <api/Sampler.h>
#include <Render/GlTexture.h>

namespace scc
{
	class GL_Texture final : public scr::Texture
	{
	private:
		OVRFW::GlTexture m_Texture;

	public:
		GL_Texture(const scr::RenderPlatform*const r)
			:scr::Texture(r) {}

		void Create(const TextureCreateInfo& pTextureCreateInfo) override;
		void Destroy() override;

		void Bind(uint32_t mip,uint32_t layer) const override;
		void BindForWrite(uint32_t slot,uint32_t mip,uint32_t layer) const override;
		void Unbind() const override;

		void GenerateMips() override;
		void UseSampler(const std::shared_ptr<scr::Sampler>& sampler) override;
		bool ResourceInUse(int timeout) override {return true;}

		inline OVRFW::GlTexture& GetGlTexture() { return m_Texture;}

		void SetExternalGlTexture(GLuint tex_id);
private:
		GLenum TypeToGLTarget(Type type) const;
		GLenum ToBaseGLFormat(Format format) const;
		GLenum ToGLFormat(Format format) const;
		GLenum ToGLCompressedFormat(CompressionFormat format, uint32_t bytesPerPixel) const;
};
}