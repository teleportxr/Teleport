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

		std::vector<std::unique_ptr<Texture>> m_ColourTextures = { nullptr };
		std::unique_ptr<Texture> m_DepthTexture = nullptr;

		std::unique_ptr<FrameBuffer> m_ResolvedFrameBuffer = nullptr;

	public:
		FrameBuffer(Texture::Format format, Texture::SampleCount sampleCount, uint32_t width, uint32_t height)
			:m_Format(format), m_SampleCount(sampleCount), m_Width(width), m_Height(height)
		{
			if (m_SampleCount > Texture::SampleCount::SAMPLE_COUNT_1_BIT)
				m_ResolvedFrameBuffer = std::make_unique<FrameBuffer>(format, Texture::SampleCount::SAMPLE_COUNT_1_BIT, m_Width, m_Height);
		};
		
		virtual void Create() = 0;
		virtual void Destroy() = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resolve() = 0;
		virtual void UpdateFrameBufferSize(uint32_t width, uint32_t height) = 0;
		virtual void Clear(float colour[4], float depth, float stencil) = 0;

		void AddColourAttachment(const Texture& colourTexture, uint32_t attachmentIndex, bool overrideTexture = false)
		{
			if (!CheckTextureCompatibility(colourTexture))
				SCR_COUT("Incompatible texture.");
			
			if (m_ColourTextures.at(attachmentIndex) == nullptr || overrideTexture)
				m_ColourTextures.at(attachmentIndex) = std::make_unique<Texture>(colourTexture);
			else
				SCR_COUT("Can't add colour texture attachment.");
		};
		void AddDepthAttachment(const Texture& depthTexture, bool overrideTexture = false)
		{
			if (!CheckTextureCompatibility(depthTexture))
				SCR_COUT("Incompatible texture.");

			if (m_DepthTexture == nullptr || overrideTexture)
				m_DepthTexture = std::make_unique<Texture>(depthTexture);
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
