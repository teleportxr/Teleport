// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Texture.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

namespace clientrender
{
	//Interface for FrameBuffer
	class FrameBuffer : public APIObject
	{
	public:
		struct ClearColous
		{
			float colour_r;
			float colour_g;
			float colour_b;
			float colour_a;
			float depth;
			uint32_t stencil;
		};
		struct FrameBufferCreateInfo
		{
			Texture::Type type;
			Texture::Format format;
			Texture::SampleCountBit sampleCount;
			uint32_t width;
			uint32_t height;
			ClearColous clearColours;
		};

	protected:
		FrameBufferCreateInfo m_CI;

		std::vector<Texture*> m_ColourTextures;
		Texture* m_DepthTexture;

		std::unique_ptr<FrameBuffer> m_ResolvedFrameBuffer = nullptr;

	public:
		FrameBuffer(const RenderPlatform* const r)
			:APIObject(r), m_CI(), m_DepthTexture(nullptr)
		{}

		virtual ~FrameBuffer()
		{
			m_CI.type = Texture::Type::TEXTURE_UNKNOWN;
			m_CI.format = Texture::Format::FORMAT_UNKNOWN;
			m_CI.sampleCount = Texture::SampleCountBit::SAMPLE_COUNT_1_BIT;
			m_CI.width = 0;
			m_CI.height = 0;

			m_ColourTextures.clear(); 
			m_DepthTexture = nullptr;
			m_ResolvedFrameBuffer.reset();
		}

		virtual void Create(FrameBufferCreateInfo* pFrameBufferCreateInfo) = 0;
		virtual void Destroy() = 0;

		virtual void Resolve() = 0;
		virtual void UpdateFrameBufferSize(uint32_t width, uint32_t height) = 0;
		virtual void SetClear(ClearColous* pClearColours) = 0;

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	
	public:
		void AddColourAttachment(Texture& colourTexture, uint32_t attachmentIndex, bool overrideTexture = false)
		{
			if (!CheckTextureCompatibility(colourTexture))
				TELEPORT_CERR<<"Incompatible texture.\n";
			
			if (m_ColourTextures.at(attachmentIndex) == nullptr || overrideTexture)
				m_ColourTextures.at(attachmentIndex) = &colourTexture;
			else
				TELEPORT_CERR<<"Can't add colour texture attachment.\n";
		};
		void AddDepthAttachment(Texture& depthTexture, bool overrideTexture = false)
		{
			if (!CheckTextureCompatibility(depthTexture))
				TELEPORT_CERR<<"Incompatible texture.\n";

			if (m_DepthTexture == nullptr || overrideTexture)
				m_DepthTexture = &depthTexture;
			else
				TELEPORT_CERR<<"Can't add depth texture attachment\n";
		};

	private:
		bool CheckTextureCompatibility(const Texture& texture)
		{
			if (m_CI.type != texture.m_CI.type ||
				m_CI.format != texture.m_CI.format ||
				m_CI.sampleCount != texture.m_CI.sampleCount)
				return false;

			if (m_CI.width != texture.m_CI.width || m_CI.height != texture.m_CI.height)
			{
				UpdateFrameBufferSize(texture.m_CI.width, texture.m_CI.height);
				return true;
			}
			else
				return false;
		}
	};
}
