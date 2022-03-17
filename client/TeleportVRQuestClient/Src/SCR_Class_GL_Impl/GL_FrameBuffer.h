// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/FrameBuffer.h>
#include <ClientRender/Texture.h>
//#include <EyeBuffers.h>

namespace scc
{
	//Implementation of FrameBuffer wrapping over ovrEyeBuffers
class GL_FrameBuffer final : public clientrender::FrameBuffer
	{
	private:
	    int eyeNum = 0;
        //OVR::ovrEyeBufferParms m_EyeBuffersParms;
	    //OVR::ovrEyeBuffers m_EyeBuffers;

	public:
		GL_FrameBuffer(const clientrender::RenderPlatform*const r)
			:clientrender::FrameBuffer(r) {}

		void Create(FrameBufferCreateInfo* pFrameBufferCreateInfo) override;
		void Destroy() override;

		void Bind() const override;
		void Unbind() const override;

		void Resolve() override;
		void UpdateFrameBufferSize(uint32_t width, uint32_t height) override;
		void SetClear(ClearColous* pClearColours) override;


		void BeginFrame();
		void EndFrame();
		//inline OVR::ovrEyeBuffers& GetOVREyeBuffers() {return m_EyeBuffers;}
	};
}
