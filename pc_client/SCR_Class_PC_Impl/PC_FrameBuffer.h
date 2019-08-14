// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/FrameBuffer.h>
#include <api/Texture.h>

namespace scr
{
	//Implementation of FrameBuffer wrapping over ovrEyeBuffers
	class PC_FrameBuffer final : public FrameBuffer
	{
	private:

	public:
		PC_FrameBuffer() {}

		void Create(Texture::Format format, Texture::SampleCountBit sampleCount, uint32_t width, uint32_t height) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		void Resolve() override;
		void UpdateFrameBufferSize(uint32_t width, uint32_t height) override;
		void Clear(float colour_r, float colour_g, float colour_b, float colour_a, float depth, uint32_t stencil) override;
	};
}
