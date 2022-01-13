#pragma once

#include <ClientRender/Camera.h>

#include "SCR_Class_GL_Impl/GL_Effect.h"
#include "SCR_Class_GL_Impl/GL_RenderPlatform.h"
#include "SCR_Class_GL_Impl/GL_Sampler.h"
#include "SCR_Class_GL_Impl/GL_Skin.h"

struct PerMeshInstanceData
{
	avs::vec4 u_LightmapScaleOffset;
};

struct GlobalGraphicsResources
{
public:
	GlobalGraphicsResources();
	void Init();
	GLint maxFragTextureSlots = 0, maxFragUniformBlocks = 0;

    scc::GL_RenderPlatform renderPlatform;
    std::shared_ptr<clientrender::Sampler> sampler;
	std::shared_ptr<clientrender::Sampler> noMipsampler;
    std::shared_ptr<clientrender::Sampler> cubeMipMapSampler;
	std::shared_ptr<clientrender::ShaderStorageBuffer> mTagDataBuffer;
	clientrender::ShaderResource lightCubemapShaderResources;
	clientrender::ShaderResource tagShaderResource;

	scc::GL_Effect defaultPBREffect;
	scc::GL_Skin defaultSkin;

    std::shared_ptr<clientrender::Camera> scrCamera;

    std::string effectPassName; //Which effect pass the geometry should be rendered with.

    static constexpr const char* HIGHLIGHT_APPEND = "_Highlight"; //What is appended on pass names to designate them a highlight pass.

    static GlobalGraphicsResources& GetInstance();

	static std::string GenerateShaderPassName(bool lightmap, bool diffuse,bool normal,bool combined,bool emissive,int lightcount,bool highlight);
private:
	static GlobalGraphicsResources *instance;
};
