// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/ShaderStorageBuffer.h>
#include <Render/GlBuffer.h>
//#include <Render/OVR_GlUtils.h>

namespace scc
{
    class GL_ShaderStorageBuffer final : public scr::ShaderStorageBuffer
    {
    private:
        OVRFW::GlBuffer m_SSBO;

    public:
        GL_ShaderStorageBuffer(const scr::RenderPlatform*const r)
                :scr::ShaderStorageBuffer(r) {}

        void Create(ShaderStorageBufferCreateInfo* pShaderStorageBufferCreateInfo) override;
        void Update(size_t size, const void* data, uint32_t offset = 0) override;
        void* Map() override;
        void Unmap() override;
        void Destroy() override;

        void Bind() const override;
        void Unbind() const override;

        void Access() override;
        bool ResourceInUse(int timeout) override {return true;}

        inline OVRFW::GlBuffer& GetGlBuffer() { return m_SSBO; }
    };
}