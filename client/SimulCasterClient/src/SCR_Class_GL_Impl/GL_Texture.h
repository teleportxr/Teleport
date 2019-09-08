// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "GL_Sampler.h"
#include <api/Texture.h>
#include <api/Sampler.h>
#include <GlTexture.h>

namespace scc
{
	class GL_Texture final : public scr::Texture
	{
	private:
		OVR::GlTexture m_Texture;

	public:
		GL_Texture(scr::RenderPlatform* r)
			:scr::Texture(r) {}

		void Create(TextureCreateInfo* pTextureCreateInfo) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		void GenerateMips() override;
		void UseSampler(std::shared_ptr<const scr::Sampler> sampler) override;
		bool ResourceInUse(int timeout) override {return true;}

		inline OVR::GlTexture& GetGlTexture() { return  m_Texture;}


private:
		GLenum TypeToGLTarget(Type type) const;
		GLenum ToBaseGLFormat(Format format) const;
		GLenum ToGLFormat(Format format) const;
		GLenum ToGLCompressedFormat(CompressionFormat format, uint32_t bytesPerPixel) const;
};
}