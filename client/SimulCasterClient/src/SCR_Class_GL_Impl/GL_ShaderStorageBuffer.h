// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/ShaderStorageBuffer.h>
#include <GlBuffer.h>
#include <OVR_GlUtils.h>

namespace scc
{
    class GL_ShaderStorageBuffer final : public scr::ShaderStorageBuffer
    {
    private:
        OVR::GlBuffer m_SSBO;

    public:
        GL_ShaderStorageBuffer(const scr::RenderPlatform*const r)
                :scr::ShaderStorageBuffer(r) {}

        void Create(ShaderStorageBufferCreateInfo* pShaderStorageBufferCreateInfo) override;
        void Destroy() override;

        void Bind() const override;
        void Unbind() const override;

        void Access() override;
        bool ResourceInUse(int timeout) override {return true;}

        inline OVR::GlBuffer& GetGlBuffer() { return m_SSBO; }
    };
}