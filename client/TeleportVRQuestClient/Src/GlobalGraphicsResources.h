#pragma once

#include <crossplatform/Camera.h>

#include "SCR_Class_GL_Impl/GL_Effect.h"
#include "SCR_Class_GL_Impl/GL_RenderPlatform.h"
#include "SCR_Class_GL_Impl/GL_Sampler.h"
#include "SCR_Class_GL_Impl/GL_Skin.h"

struct GlobalGraphicsResources
{
public:
	GlobalGraphicsResources();
	GLint maxFragTextureSlots = 0, maxFragUniformBlocks = 0;

    scc::GL_RenderPlatform renderPlatform;
    std::shared_ptr<scr::Sampler> sampler;
    std::shared_ptr<scr::Sampler> cubeMipMapSampler;
	std::shared_ptr<scr::ShaderStorageBuffer> mTagDataBuffer;
	scr::ShaderResource lightCubemapShaderResources;
	scr::ShaderResource tagShaderResource;

	scc::GL_Effect defaultPBREffect;
	scc::GL_Skin defaultSkin;

    std::shared_ptr<scr::Camera> scrCamera;

    char* effectPassName = const_cast<char*>("OpaquePBR"); //Which effect pass the geometry should be rendered with.

    static GlobalGraphicsResources& GetInstance();

	static std::string GenerateShaderPassName(int diffuse,int normal,int combined,int emissive);
private:
	static GlobalGraphicsResources *instance;

};
