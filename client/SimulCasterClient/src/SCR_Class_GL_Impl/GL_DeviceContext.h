// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/DeviceContext.h>
#include <crossplatform/DescriptorSet.h>

#include "GL_FrameBuffer.h"
#include "GL_VertexBuffer.h"
#include "GL_IndexBuffer.h"
#include "GL_Effect.h"
#include "GL_Texture.h"
#include "GL_UniformBuffer.h"

namespace scc
{
class GL_DeviceContext final : public scr::DeviceContext
    {
	public:
    GL_DeviceContext(scr::RenderPlatform *r)
        :scr::DeviceContext(r) {}

        void Create(DeviceContextCreateInfo* pDeviceContextCreateInfo) override;

        void Draw(scr::InputCommand* pInputCommand) override;
        void DispatchCompute(scr::InputCommand* pInputCommand) override;

        void BeginFrame() override;
        void EndFrame() override;

    private:
    	void ParseInputCommand(scr::InputCommand* pInputCommand);
        void BindDescriptorSets(const std::vector<scr::DescriptorSet>& descriptorSets, scr::Effect* pEffect);

    private:
    	GLenum m_Topology;
        GLsizei m_IndexCount;
        GLenum m_Type; //Stride of the data
	};
}