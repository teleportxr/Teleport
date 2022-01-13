// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/DeviceContext.h>
#include <ClientRender/ShaderResource.h>

#include "GL_Effect.h"
#include "GL_FrameBuffer.h"
#include "GL_IndexBuffer.h"
#include "GL_ShaderStorageBuffer.h"
#include "GL_Texture.h"
#include "GL_UniformBuffer.h"
#include "GL_VertexBuffer.h"

namespace scc
{
	class GL_DeviceContext final : public clientrender::DeviceContext
    {
	public:
    GL_DeviceContext(const clientrender::RenderPlatform*const r)
        :clientrender::DeviceContext(r) {}

        void Create(DeviceContextCreateInfo* pDeviceContextCreateInfo) override;

        void Draw(clientrender::InputCommand* pInputCommand) override;
        void DispatchCompute(clientrender::InputCommand* pInputCommand) override;

        void BeginFrame() override;
        void EndFrame() override;

    private:
        void BindShaderResources(const std::vector<const clientrender::ShaderResource*>& shaderResources, clientrender::Effect* pEffect, const char* effectPassName);

    private:
    	GLenum m_Topology;
        GLsizei m_IndexCount;
        GLenum m_Type; //Stride of the data
	};
}