// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "PC_Sampler.h"
#include <ClientRender/Texture.h>
#include <ClientRender/Sampler.h>
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		class Texture;
	}
}

namespace pc_client
{
	class PC_Texture final : public clientrender::Texture
	{
	private:
		simul::crossplatform::Texture* m_SimulTexture;
	public:
		PC_Texture(const clientrender::RenderPlatform*const r)
			:clientrender::Texture(r), m_SimulTexture(nullptr)
		{}

		virtual ~PC_Texture();
		void Destroy() override;

		void Bind(uint32_t mip,uint32_t index) const override;
		void BindForWrite(uint32_t slot,uint32_t mip,uint32_t index) const override;
		void Unbind() const override;

		void GenerateMips() override;
		void UseSampler(const std::shared_ptr<clientrender::Sampler>& sampler) override;
		bool ResourceInUse(int timeout) override {return true;}

		// Inherited via Texture
		void Create(const TextureCreateInfo& pTextureCreateInfo) override;

		simul::crossplatform::Texture* GetSimulTexture()
		{
			return m_SimulTexture;
		}
	};
}