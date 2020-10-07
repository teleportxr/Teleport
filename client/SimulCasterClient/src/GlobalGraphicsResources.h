#pragma once

#include "SCR_Class_GL_Impl/GL_RenderPlatform.h"
#include "SCR_Class_GL_Impl/GL_Sampler.h"

struct GlobalGraphicsResources
{
public:
	GLint maxFragTextureSlots = 0, maxFragUniformBlocks = 0;

	scc::GL_Effect *GetPbrEffect();

    scc::GL_RenderPlatform renderPlatform;
    scc::GL_Effect pbrEffect;
    std::shared_ptr<scr::Sampler> sampler;
    std::shared_ptr<scr::Sampler> cubeMipMapSampler;
    scr::ShaderResource lightCubemapShaderResources;

    std::shared_ptr<scr::Camera> scrCamera;

    char* effectPassName = const_cast<char*>("OpaquePBR"); //Which effect pass the geometry should be rendered with.

    static GlobalGraphicsResources& GetInstance()
    {
        return instance;
    }
private:
    GlobalGraphicsResources():pbrEffect(&renderPlatform)
    {}

    static GlobalGraphicsResources instance;
};