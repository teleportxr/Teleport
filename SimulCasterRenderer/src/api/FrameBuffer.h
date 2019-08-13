// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Texture.h"

namespace scr
{
	//Interface for FrameBuffer
	class FrameBuffer
	{
	protected:
		Texture::Type m_Type = Texture::Type::TEXTURE_2D;
		Texture::Format m_Format;
		Texture::SampleCount m_SampleCount;
		uint32_t m_Width, m_Height;

		std::vector<Texture*> m_ColourTextures;
		Texture* m_DepthTexture;

		std::unique_ptr<FrameBuffer> m_ResolvedFrameBuffer = nullptr;

	public:
		virtual ~FrameBuffer()
		{
			m_ResolvedFrameBuffer.reset();
		}

		virtual void Create(Texture::Format format, Texture::SampleCount sampleCount, uint32_t width, uint32_t height) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void Resolve() = 0;
		virtual void UpdateFrameBufferSize(uint32_t width, uint32_t height) = 0;
		virtual void Clear(float colour_r, float colour_g, float colour_b, float colour_a, float depth, uint32_t stencil) = 0;

		void AddColourAttachment(Texture& colourTexture, uint32_t attachmentIndex, bool overrideTexture = false)
		{
			if (!CheckTextureCompatibility(colourTexture))
				SCR_COUT("Incompatible texture.");
			
			if (m_ColourTextures.at(attachmentIndex) == nullptr || overrideTexture)
				m_ColourTextures.at(attachmentIndex) = &colourTexture;
			else
				SCR_COUT("Can't add colour texture attachment.");
		};
		void AddDepthAttachment(Texture& depthTexture, bool overrideTexture = false)
		{
			if (!CheckTextureCompatibility(depthTexture))
				SCR_COUT("Incompatible texture.");

			if (m_DepthTexture == nullptr || overrideTexture)
				m_DepthTexture = &depthTexture;
			else
				SCR_COUT("Can't add depth texture attachment");
		};

	private:
		bool CheckTextureCompatibility(const Texture& texture)
		{
			if (m_Type != texture.m_Type ||
				m_Format != texture.m_Format ||
				m_SampleCount != texture.m_SampleCount)
				return false;

			if (m_Width != texture.m_Width || m_Height != texture.m_Height)
			{
				UpdateFrameBufferSize(texture.m_Width, texture.m_Height);
				return true;
			}
			else
				return false;
		}
	};
}
