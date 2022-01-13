// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/FrameBuffer.h>
#include <ClientRender/Texture.h>

namespace pc_client
{
	//Implementation of FrameBuffer wrapping over ovrEyeBuffers
	class PC_FrameBuffer final : public clientrender::FrameBuffer
	{
	private:

	public:
		PC_FrameBuffer(const clientrender::RenderPlatform *r):clientrender::FrameBuffer(r) {}

		// Inherited via FrameBuffer
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		void Resolve() override;
		void UpdateFrameBufferSize(uint32_t width, uint32_t height) override;

		void Create(FrameBufferCreateInfo * pFrameBufferCreateInfo) override;
		void SetClear(ClearColous * pClearColours) override;
	};
}
