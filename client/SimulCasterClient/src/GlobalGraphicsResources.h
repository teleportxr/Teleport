#pragma once

#include "SCR_Class_GL_Impl/GL_RenderPlatform.h"
#include "SCR_Class_GL_Impl/GL_Sampler.h"

struct GlobalGraphicsResources
{
public:
    GLint maxFragTextureSlots = 0, maxFragUniformBlocks = 0;

    scc::GL_RenderPlatform renderPlatform;
    scc::GL_Effect pbrEffect = dynamic_cast<scr::RenderPlatform*>(&renderPlatform);
    std::shared_ptr<scr::Sampler> sampler;
    std::shared_ptr<scr::Sampler> cubeMipMapSampler;
    scr::ShaderResource lightCubemapShaderResources;

    std::shared_ptr<scr::Camera> scrCamera;

    char* effectPassName = const_cast<char*>("OpaquePBR"); //Which effect pass the geometry should be rendered with.
    bool is_clockwise_winding = false; //Whether the geometry winding order is clockwise.

    static GlobalGraphicsResources& GetInstance()
    {
        return instance;
    }
private:
    GlobalGraphicsResources()
    {}

    static GlobalGraphicsResources instance;
};