// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "PC_Sampler.h"
#include <api/Texture.h>
#include <api/Sampler.h>

namespace pc_client
{
	class PC_Texture final : public scr::Texture
	{
	private:

	public:
		PC_Texture() {}

		void Create(scr::Texture::Slot slot, scr::Texture::Type type, scr::Texture::Format format, scr::Texture::SampleCount sampleCount, uint32_t width, uint32_t height, uint32_t depth, uint32_t bitsPerPixel, const uint8_t* data) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		void GenerateMips() override;
		void UseSampler(const scr::Sampler* sampler) override;
		bool ResourceInUse(int timeout) override {return true;}

	private:
	};
}