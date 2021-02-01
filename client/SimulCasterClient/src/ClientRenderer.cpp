//
// Created by roder on 06/04/2020.
//
#include "ClientRenderer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "OVR_GlUtils.h"
#include "OVR_Math.h"
#include <VrApi_Types.h>
#include <VrApi_Input.h>

#include "OVRNodeManager.h"
#include "ClientDeviceState.h"

static const char *ToString(scr::Light::Type type)
{
	const char *lightTypeName="";
	switch(type)
	{
		case scr::Light::Type::POINT:
			lightTypeName="Point";
			break;
		case scr::Light::Type::DIRECTIONAL:
			lightTypeName="  Dir";
			break;
		case scr::Light::Type::SPOT:
			lightTypeName=" Spot";
			break;
		case scr::Light::Type::AREA:
			lightTypeName=" Area";
			break;
	};
	return lightTypeName;
}

using namespace OVR;
extern ovrQuatf QuaternionMultiply(const ovrQuatf &p,const ovrQuatf &q);
avs::vec3 QuaternionTimesVector(const ovrQuatf &q,const avs::vec3 &vec)
{
	const float &x0 = vec.x;
	const float &y0 = vec.y;
	const float &z0 = vec.z;
	float s1 = q.x * x0 + q.y * y0 + q.z * z0;
	float x1 = q.w * x0 + q.y * z0 - q.z * y0;
	float y1 = q.w * y0 + q.z * x0 - q.x * z0;
	float z1 = q.w * z0 + q.x * y0 - q.y * x0;
	avs::vec3 ret={ s1 * q.x + q.w * x1 + q.y * z1 - q.z * y1,
					s1 * q.y + q.w * y1 + q.z * x1 - q.x * z1,
					s1 * q.z + q.w * z1 + q.x * y1 - q.y * x1};
	return ret;
}
ClientRenderer::ClientRenderer(ResourceCreator *r,scr::ResourceManagers *rm,SessionCommandInterface *i,ClientAppInterface *c
,ClientDeviceState *s)
		:mDecoder(avs::DecoderBackend::Custom)
		, resourceManagers(rm)
		, resourceCreator(r)
		, clientAppInterface(c)
		, mOvrMobile(nullptr)
        , mVideoSurfaceTexture(nullptr)
        , mCubemapTexture(nullptr)
        , mCubemapLightingTexture(nullptr)
        , mTagDataIDBuffer(nullptr)
		, mTagDataArrayBuffer(nullptr)
		, mTagDataBuffer(nullptr)
		, clientDeviceState(s)
{
}


ClientRenderer::~ClientRenderer()
{
	ExitedVR();
}

void ClientRenderer::EnteredVR(struct ovrMobile *o,const ovrJava *java)
{
	mOvrMobile=o;
	//VideoSurfaceProgram
	{
		{
			mVideoUB=GlobalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
			scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo={2, sizeof(VideoUB)
			                                                                     , &videoUB};
			mVideoUB->Create(&uniformBufferCreateInfo);
		}
		static ovrProgramParm uniformParms[]  =
				                      {
						                       {"cubemapTexture"   , ovrProgramParmType::TEXTURE_SAMPLED}
						                      , {"videoUB"          , ovrProgramParmType::BUFFER_UNIFORM}
                                              , {"TagDataCube"  , ovrProgramParmType::BUFFER_STORAGE}
						              ,};
		std::string           videoSurfaceVert=clientAppInterface->LoadTextFile(
				"shaders/VideoSurface.vert");
		std::string           videoSurfaceFrag=clientAppInterface->LoadTextFile(
				"shaders/VideoSurface.frag");
		mVideoSurfaceProgram=GlProgram::Build(
				nullptr, videoSurfaceVert.c_str()
				, "#extension GL_OES_EGL_image_external_essl3 : require\n", videoSurfaceFrag.c_str()
				, uniformParms, sizeof(uniformParms)/sizeof(ovrProgramParm), 310);
		if(!mVideoSurfaceProgram.IsValid())
		{
			OVR_FAIL("Failed to build video surface shader program");
		}
	}
	{
		mVideoSurfaceTexture =new OVR::SurfaceTexture(java->Env);
		mVideoTexture        =GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mCubemapUB           =GlobalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
		mCubemapTexture      =GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		diffuseCubemapTexture      =GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		specularCubemapTexture     =GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mRoughSpecularTexture=GlobalGraphicsResources.renderPlatform.InstantiateTexture();

		mCubemapLightingTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mTagDataIDBuffer = GlobalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
		mTagDataArrayBuffer = GlobalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
		mTagDataBuffer = GlobalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
	}
	// Tag Data ID
	{
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo shaderStorageBufferCreateInfo = {
				0,
				scr::ShaderStorageBuffer::Access::NONE,
					sizeof(scr::uvec4),
				(void*)&mTagDataID
		};
		mTagDataIDBuffer->Create(&shaderStorageBufferCreateInfo);
	}

	if (mIsCubemapVideo)
	{
		// Tag Data Cube Buffer
		VideoTagDataCube shaderTagDataCubeArray[MAX_TAG_DATA_COUNT];
		shaderTagDataCubeArray[0].cameraPosition.x=1.0f;
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo tagBufferCreateInfo = {
				1,
				scr::ShaderStorageBuffer::Access::READ_WRITE_BIT,
				sizeof(VideoTagDataCube),
				(void*) nullptr
		};
		mTagDataBuffer->Create(&tagBufferCreateInfo);

		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo arrayBufferCreateInfo = {
				2,
				scr::ShaderStorageBuffer::Access::READ_WRITE_BIT,
				sizeof(VideoTagDataCube) * MAX_TAG_DATA_COUNT,
				(void*)&shaderTagDataCubeArray
		};
		mTagDataArrayBuffer->Create(&arrayBufferCreateInfo);
	}
	else
	{
		// Tag Data 2D Buffer
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo tagBufferCreateInfo = {
				1,
				scr::ShaderStorageBuffer::Access::READ_WRITE_BIT,
				sizeof(VideoTagData2D),
				(void*)nullptr
		};
		mTagDataBuffer->Create(&tagBufferCreateInfo);
		VideoTagData2D shaderTagData2DArray[MAX_TAG_DATA_COUNT];
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo arrayBufferCreateInfo = {
				2,
				scr::ShaderStorageBuffer::Access::NONE,
				sizeof(VideoTagData2D) * MAX_TAG_DATA_COUNT,
				(void*)&shaderTagData2DArray
		};
		mTagDataArrayBuffer->Create(&arrayBufferCreateInfo);

	}

	{
		CopyCubemapSrc     = clientAppInterface->LoadTextFile("shaders/CopyCubemap.comp");
		mCopyCubemapEffect = GlobalGraphicsResources.renderPlatform.InstantiateEffect();
		mCopyCubemapWithDepthEffect = GlobalGraphicsResources.renderPlatform.InstantiateEffect();
		mExtractTagDataIDEffect=GlobalGraphicsResources.renderPlatform.InstantiateEffect();
		mExtractOneTagEffect=GlobalGraphicsResources.renderPlatform.InstantiateEffect();
		scr::Effect::EffectCreateInfo effectCreateInfo = {};
		effectCreateInfo.effectName = "CopyCubemap";
		mCopyCubemapEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "CopyCubemapWithDepth";
		mCopyCubemapWithDepthEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "ExtractTagDataID";
		mExtractTagDataIDEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "ExtractOneTag";
		mExtractOneTagEffect->Create(&effectCreateInfo);

		scr::ShaderSystem::PipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.m_Count = 1;
		pipelineCreateInfo.m_PipelineType                   = scr::ShaderSystem::PipelineType::PIPELINE_TYPE_COMPUTE;
		pipelineCreateInfo.m_ShaderCreateInfo[0].stage      = scr::Shader::Stage::SHADER_STAGE_COMPUTE;
		pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "colour_only";
		pipelineCreateInfo.m_ShaderCreateInfo[0].filepath   = "shaders/CopyCubemap.comp";
		pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = CopyCubemapSrc;
		scr::ShaderSystem::Pipeline cp(&GlobalGraphicsResources.renderPlatform, &pipelineCreateInfo);

		scr::Effect::EffectPassCreateInfo effectPassCreateInfo;
		effectPassCreateInfo.effectPassName = "CopyCubemap";
		effectPassCreateInfo.pipeline = cp;
		mCopyCubemapEffect->CreatePass(&effectPassCreateInfo);

		pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "colour_and_depth";
		scr::ShaderSystem::Pipeline cp2(&GlobalGraphicsResources.renderPlatform, &pipelineCreateInfo);

		effectPassCreateInfo.effectPassName = "ColourAndDepth";
		effectPassCreateInfo.pipeline = cp2;
		mCopyCubemapWithDepthEffect->CreatePass(&effectPassCreateInfo);

		{
			ExtractTagDataIDSrc = clientAppInterface->LoadTextFile("shaders/ExtractTagDataID.comp");
			pipelineCreateInfo.m_ShaderCreateInfo[0].filepath   = "shaders/ExtractTagDataID.comp";
			pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = ExtractTagDataIDSrc;
			pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "extract_tag_data_id";
			scr::ShaderSystem::Pipeline cp3(&GlobalGraphicsResources.renderPlatform, &pipelineCreateInfo);
			effectPassCreateInfo.effectPassName = "ExtractTagDataID";
			effectPassCreateInfo.pipeline = cp3;
			mExtractTagDataIDEffect->CreatePass(&effectPassCreateInfo);

			std::string ExtractTagDataSrc = clientAppInterface->LoadTextFile("shaders/ExtractOneTag.comp");
			// pass to extract from the array into a single tag buffer:
			pipelineCreateInfo.m_ShaderCreateInfo[0].filepath   = "shaders/ExtractOneTag.comp";
			pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = ExtractTagDataSrc;
			pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "extract_tag_data";
			scr::ShaderSystem::Pipeline cp4(&GlobalGraphicsResources.renderPlatform, &pipelineCreateInfo);
			effectPassCreateInfo.effectPassName = "ExtractOneTag";
			effectPassCreateInfo.pipeline = cp4;
			mExtractOneTagEffect->CreatePass(&effectPassCreateInfo);

			scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo = {2, sizeof(CubemapUB), &cubemapUB};
			mCubemapUB->Create(&uniformBufferCreateInfo);
		}
		GL_CheckErrors("mCubemapUB:Create");

		scr::ShaderResourceLayout layout;
		layout.AddBinding(0, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, scr::Shader::Stage ::SHADER_STAGE_COMPUTE);
		layout.AddBinding(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage ::SHADER_STAGE_COMPUTE);
		layout.AddBinding(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage ::SHADER_STAGE_COMPUTE);
		layout.AddBinding(3, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, scr::Shader::Stage::SHADER_STAGE_COMPUTE);

		scr::ShaderResource sr({layout, layout, layout});
		sr.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, 0, "destTex", {GlobalGraphicsResources.cubeMipMapSampler, mCubemapTexture,0,uint32_t(-1)});
		sr.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1, "videoFrameTexture", {GlobalGraphicsResources.sampler, mVideoTexture});
		sr.AddBuffer(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB", {mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});

		sr.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, 0, "destTex", {});
		sr.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1, "videoFrameTexture", {GlobalGraphicsResources.sampler, mVideoTexture});
		sr.AddBuffer(1, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB", {mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});

		sr.AddImage(2, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1, "videoFrameTexture", {GlobalGraphicsResources.sampler, mVideoTexture});
		sr.AddBuffer(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB", {mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});
		sr.AddBuffer(2, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 0, "TagDataID", {mTagDataIDBuffer.get()});
		sr.AddBuffer(2, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 1, "TagDataCube_ssbo", {mTagDataBuffer.get()});
		sr.AddBuffer(2, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 2, "TagDataCubeArray_ssbo", {mTagDataArrayBuffer.get()});

		mCubemapComputeShaderResources.push_back(sr);

		mCopyCubemapEffect->LinkShaders("CopyCubemap", {});
		mCopyCubemapWithDepthEffect->LinkShaders("ColourAndDepth",{});
		mExtractTagDataIDEffect->LinkShaders("ExtractTagDataID",{});
		mExtractOneTagEffect->LinkShaders("ExtractOneTag",{});
	}

	mVideoSurfaceDef.surfaceName = "VideoSurface";
	mVideoSurfaceDef.geo = BuildGlobe(1.f,1.f,500.f);
	//BuildTesselatedQuad( 1, 1,true );
	mVideoSurfaceDef.graphicsCommand.Program = mVideoSurfaceProgram;
	mVideoSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
	mVideoSurfaceDef.graphicsCommand.GpuState.cullEnable = false;
	//Set up scr::Camera
	scr::Camera::CameraCreateInfo c_ci = {
			(scr::RenderPlatform*)(&GlobalGraphicsResources.renderPlatform),
			scr::Camera::ProjectionType::PERSPECTIVE,
			scr::quat(0.0f, 0.0f, 0.0f, 1.0f),
			cameraPosition
	};
	GlobalGraphicsResources.scrCamera = std::make_shared<scr::Camera>(&c_ci);

	scr::VertexBufferLayout layout;
	layout.AddAttribute(0, scr::VertexBufferLayout::ComponentCount::VEC3, scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(1, scr::VertexBufferLayout::ComponentCount::VEC3, scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(2, scr::VertexBufferLayout::ComponentCount::VEC4, scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(3, scr::VertexBufferLayout::ComponentCount::VEC2, scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(4, scr::VertexBufferLayout::ComponentCount::VEC2, scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(5, scr::VertexBufferLayout::ComponentCount::VEC4, scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(6, scr::VertexBufferLayout::ComponentCount::VEC4, scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(7, scr::VertexBufferLayout::ComponentCount::VEC4, scr::VertexBufferLayout::Type::FLOAT);
	layout.CalculateStride();

	scr::ShaderResourceLayout vertLayout;
	vertLayout.AddBinding(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_VERTEX);
	vertLayout.AddBinding(3, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_VERTEX);
	vertLayout.AddBinding(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_VERTEX);
	scr::ShaderResourceLayout fragLayout;
	fragLayout.AddBinding(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(10, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(11, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(12, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(13, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(14, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(15, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(16, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	fragLayout.AddBinding(17, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);

	scr::ShaderResource pbrShaderResource({vertLayout, fragLayout});
	pbrShaderResource.AddBuffer(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 0, "u_CameraData", {});
	pbrShaderResource.AddBuffer(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 3, "u_BoneData", {});
	pbrShaderResource.AddBuffer(1, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "u_MaterialData", {});
	pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 10, "u_DiffuseTexture", {});
	pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 11, "u_NormalTexture", {});
	pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 12, "u_CombinedTexture", {});
	pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 13, "u_EmissiveTexture", {});
	pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 14, "u_DiffuseCubemap", {});
	pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 15, "u_SpecularCubemap", {});
	pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 16, "u_RoughSpecularCubemap", {});

	passNames.clear();
	passNames.push_back("OpaquePBR");
	passNames.push_back("OpaqueAlbedo");
	passNames.push_back("OpaqueNormal");

	scr::ShaderSystem::PipelineCreateInfo pipelineCreateInfo;
	{
		pipelineCreateInfo.m_Count                          = 2;
		pipelineCreateInfo.m_PipelineType                   = scr::ShaderSystem::PipelineType::PIPELINE_TYPE_GRAPHICS;
		pipelineCreateInfo.m_ShaderCreateInfo[0].stage      = scr::Shader::Stage::SHADER_STAGE_VERTEX;
		pipelineCreateInfo.m_ShaderCreateInfo[0].filepath   = "shaders/OpaquePBR.vert";
		pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = clientAppInterface->LoadTextFile("shaders/OpaquePBR.vert");
		pipelineCreateInfo.m_ShaderCreateInfo[1].stage      = scr::Shader::Stage::SHADER_STAGE_FRAGMENT;
		pipelineCreateInfo.m_ShaderCreateInfo[1].filepath   = "shaders/OpaquePBR.frag";
		pipelineCreateInfo.m_ShaderCreateInfo[1].sourceCode = clientAppInterface->LoadTextFile("shaders/OpaquePBR.frag");
	}

	//Static passes.
	pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "Static";
	pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = "OpaquePBR";
	clientAppInterface->BuildEffectPass("Static_OpaquePBR", &layout, &pipelineCreateInfo, {pbrShaderResource});

	pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = "OpaqueAlbedo";
	clientAppInterface->BuildEffectPass("Static_OpaqueAlbedo", &layout, &pipelineCreateInfo, {pbrShaderResource});

	pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = "OpaqueNormal";
	clientAppInterface->BuildEffectPass("Static_OpaqueNormal", &layout, &pipelineCreateInfo, {pbrShaderResource});

	//Skinned passes.
	pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "Animated";
	pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = "OpaquePBR";
	clientAppInterface->BuildEffectPass("Animated_OpaquePBR", &layout, &pipelineCreateInfo, {pbrShaderResource});

	pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = "OpaqueAlbedo";
	clientAppInterface->BuildEffectPass("Animated_OpaqueAlbedo", &layout, &pipelineCreateInfo, {pbrShaderResource});

	pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = "OpaqueNormal";
	clientAppInterface->BuildEffectPass("Animated_OpaqueNormal", &layout, &pipelineCreateInfo, {pbrShaderResource});
}

void ClientRenderer::ExitedVR()
{
	mOvrMobile=nullptr;
	delete mVideoSurfaceTexture;
	mVideoSurfaceDef.geo.Free();
	GlProgram::Free(mVideoSurfaceProgram);
}

void ClientRenderer::OnVideoStreamChanged(const avs::VideoConfig &vc)
{
	videoConfig=vc;
	//Build Video Cubemap
	{
		scr::Texture::TextureCreateInfo textureCreateInfo =
				{
						"Cubemap Texture",
						videoConfig.colour_cubemap_size,
						videoConfig.colour_cubemap_size,
						1,
						4,
						1,
						1,
						scr::Texture::Slot::UNKNOWN,
						scr::Texture::Type::TEXTURE_CUBE_MAP,
						scr::Texture::Format::RGBA8,
						scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT,
						{},
						{},
						scr::Texture::CompressionFormat::UNCOMPRESSED
				};
		mCubemapTexture->Create(textureCreateInfo);
		mCubemapTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
	}
	//GL_CheckErrors("Built Video Cubemap");
	//Build Lighting Cubemap
	{
		scr::Texture::TextureCreateInfo textureCreateInfo //TODO: Check this against the incoming texture from the video stream
				{
						"Cubemap Sub-Textures",
						128,
						128,
						1,
						4,
						1,
						3,
						scr::Texture::Slot::UNKNOWN,
						scr::Texture::Type::TEXTURE_CUBE_MAP,
						scr::Texture::Format::RGBA8,
						scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT,
						{},
						{},
						scr::Texture::CompressionFormat::UNCOMPRESSED
				};
		textureCreateInfo.mipCount = 1;
		textureCreateInfo.width  = videoConfig.diffuse_cubemap_size;
		textureCreateInfo.height = videoConfig.diffuse_cubemap_size;
		diffuseCubemapTexture->Create(textureCreateInfo);
		textureCreateInfo.width  = videoConfig.light_cubemap_size;
		textureCreateInfo.height = videoConfig.light_cubemap_size;
		mCubemapLightingTexture->Create(textureCreateInfo);
		textureCreateInfo.mipCount = 3;
		textureCreateInfo.width  = videoConfig.specular_cubemap_size;
		textureCreateInfo.height = videoConfig.specular_cubemap_size;
		specularCubemapTexture->Create(textureCreateInfo);
		textureCreateInfo.width  = videoConfig.rough_cubemap_size;
		textureCreateInfo.height = videoConfig.rough_cubemap_size;
		mRoughSpecularTexture->Create(textureCreateInfo);
		diffuseCubemapTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
		specularCubemapTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
		mRoughSpecularTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
		mCubemapLightingTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
	}
	//GL_CheckErrors("Built Lighting Cubemap");
}
void ClientRenderer::OnReceiveVideoTagData(const uint8_t* data, size_t dataSize)
{
	if (lastSetupCommand.video_config.use_cubemap)
	{
		scr::SceneCaptureCubeTagData tagData;
		memcpy(&tagData.coreData, data, sizeof(scr::SceneCaptureCubeCoreTagData));
		avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::GlStyle, tagData.coreData.cameraTransform);

		tagData.lights.resize(std::min(tagData.coreData.lightCount,(uint32_t)4));

		// Aidan : View and proj matrices are currently unchanged from Unity
		size_t index = sizeof(scr::SceneCaptureCubeCoreTagData);
		for (auto& light : tagData.lights)
		{
			memcpy(&light, &data[index], sizeof(scr::LightTagData));
			avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::GlStyle, light.worldTransform);
			index += sizeof(scr::LightTagData);
		}

		VideoTagDataCube shaderData;
		shaderData.cameraPosition = tagData.coreData.cameraTransform.position;
		shaderData.cameraRotation = tagData.coreData.cameraTransform.rotation;
		shaderData.lightCount = tagData.lights.size();

		uint32_t offset = sizeof(VideoTagDataCube) * tagData.coreData.id;
		mTagDataArrayBuffer->Update(sizeof(VideoTagDataCube), (void*)&shaderData, offset);

		videoTagDataCubeArray[tagData.coreData.id] = std::move(tagData);
	}
	else
	{
		scr::SceneCapture2DTagData tagData;
		memcpy(&tagData, data, dataSize);
		avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::GlStyle, tagData.cameraTransform);

		VideoTagData2D shaderData;
		shaderData.cameraPosition = tagData.cameraTransform.position;
		shaderData.lightCount = 0;//tagData.lights.size();
		shaderData.cameraRotation = tagData.cameraTransform.rotation;

		uint32_t offset = sizeof(VideoTagData2D) * tagData.id;
		mTagDataBuffer->Update(sizeof(VideoTagData2D), (void*)&shaderData, offset);

		mVideoTagData2DArray[tagData.id] = std::move(tagData);
	}
}
void ClientRenderer::CopyToCubemaps(scc::GL_DeviceContext &mDeviceContext)
{
	scr::ivec2 specularOffset={videoConfig.specular_x, videoConfig.specular_y};
	scr::ivec2 diffuseOffset={videoConfig.diffuse_x, videoConfig.diffuse_y};
	scr::ivec2 roughOffset={videoConfig.rough_x, videoConfig.rough_y};
	//scr::ivec2  lightOffset={2 * specularSize+3 * specularSize / 2, specularSize * 2};
	// Here the compute shader to copy from the video texture into the cubemap/s.
	auto &tc=mCubemapTexture->GetTextureCreateInfo();
	if(mCubemapTexture->IsValid())
	{
		const uint32_t ThreadCount=8;
		GLint max_u,max_v,max_w;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,&max_u);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,1,&max_v);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,2,&max_w);
		scr::uvec3 size  = {tc.width/ThreadCount, tc.width/ThreadCount, 6};

		size.x=std::min(size.x,(uint32_t)max_u);
		size.y=std::min(size.y,(uint32_t)max_v);
		size.z=std::min(size.z,(uint32_t)max_w);

		scr::InputCommandCreateInfo inputCommandCreateInfo;
		inputCommandCreateInfo.effectPassName = "ColourAndDepth";

		scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, size, mCopyCubemapWithDepthEffect, {mCubemapComputeShaderResources[0][0]});
		cubemapUB.faceSize			=tc.width;
		cubemapUB.sourceOffset		={0,0};
		cubemapUB.mip             	=0;
		cubemapUB.face             	=0;

		mDeviceContext.DispatchCompute(&inputCommand);
		GL_CheckErrors("Frame: CopyToCubemaps - Main");
		cubemapUB.faceSize = 0;
		scr::ivec2 offset0={0,0};//(int32_t) ((3 *  tc.width) / 2),(int32_t) (2 * tc.width)};
		//Lighting Cubemaps
		inputCommand.m_pComputeEffect=mCopyCubemapEffect;
		inputCommand.effectPassName = "CopyCubemap";
		int32_t mip_y=0;
		if(diffuseCubemapTexture->IsValid())
		{
			static uint32_t face= 0;
			mip_y = 0;
			int32_t mip_size=videoConfig.diffuse_cubemap_size;
			uint32_t M=diffuseCubemapTexture->GetTextureCreateInfo().mipCount;
			scr::ivec2 offset={offset0.x+diffuseOffset.x,offset0.y+diffuseOffset.y};
			for (uint32_t m        = 0; m < M; m++)
			{
				inputCommand.m_WorkGroupSize = {(mip_size + 1) / ThreadCount, (mip_size + 1) / ThreadCount ,6};
				mCubemapComputeShaderResources[0].SetImageInfo(1, 0, {diffuseCubemapTexture->GetSampler(), diffuseCubemapTexture, m});
				cubemapUB.sourceOffset			={offset.x,offset.y+mip_y};
				cubemapUB.faceSize 				= uint32_t(mip_size);
				cubemapUB.mip                  = m;
				cubemapUB.face				   = 0;
				inputCommand.m_ShaderResources = {mCubemapComputeShaderResources[0][1]};
				mDeviceContext.DispatchCompute(&inputCommand);
				//OVR_LOG("Dispatch offset=%d %d wgSize=%d %d %d mipSize=%d",cubemapUB.sourceOffset.x,cubemapUB.sourceOffset.y,inputCommand.m_WorkGroupSize.x,inputCommand.m_WorkGroupSize.y,inputCommand.m_WorkGroupSize.z,cubemapUB.faceSize);

				mip_y += 2 * mip_size;
				mip_size /= 2;
			}
			face++;
			face=face%6;
		}
		if(specularCubemapTexture->IsValid())
		{
			mip_y = 0;
			int32_t          mip_size = videoConfig.specular_cubemap_size;
			uint32_t         M        = specularCubemapTexture->GetTextureCreateInfo().mipCount;
			scr::ivec2       offset   = {
					offset0.x + specularOffset.x, offset0.y + specularOffset.y};
			for (uint32_t m        = 0; m < M; m++)
			{
				inputCommand.m_WorkGroupSize = {
						(mip_size + 1) / ThreadCount, (mip_size + 1) / ThreadCount, 6};
				mCubemapComputeShaderResources[0].SetImageInfo(
						1, 0, {specularCubemapTexture->GetSampler(), specularCubemapTexture, m});
				cubemapUB.sourceOffset         = {offset.x, offset.y + mip_y};
				cubemapUB.faceSize             = uint32_t(mip_size);
				cubemapUB.mip                  = m;
				cubemapUB.face                 = 0;
				inputCommand.m_ShaderResources = {mCubemapComputeShaderResources[0][1]};
				mDeviceContext.DispatchCompute(&inputCommand);
				mip_y += 2 * mip_size;
				mip_size /= 2;
			}
		}
		if(mRoughSpecularTexture->IsValid())
		{
			mip_y = 0;
			int32_t          mip_size = videoConfig.rough_cubemap_size;
			uint32_t         M        = specularCubemapTexture->GetTextureCreateInfo().mipCount;
			scr::ivec2       offset   = {	offset0.x + roughOffset.x, offset0.y + roughOffset.y};
			for(uint32_t m=0;m<M;m++)
			{
				inputCommand.m_WorkGroupSize={(mip_size+1)/ThreadCount,(mip_size+1)/ThreadCount,6};
				mCubemapComputeShaderResources[0].SetImageInfo(1 ,0, {mRoughSpecularTexture->GetSampler(), mRoughSpecularTexture, m});
				cubemapUB.sourceOffset			={offset.x,offset.y+mip_y};
				cubemapUB.faceSize 				= uint32_t(mip_size);
				cubemapUB.mip             		= m;
				cubemapUB.face					= 0;
				inputCommand.m_ShaderResources	= {mCubemapComputeShaderResources[0][1]};
				mDeviceContext.DispatchCompute(&inputCommand);
				mip_y							+=2*mip_size;
				mip_size/=2;
			}
		}
		GL_CheckErrors("Frame: CopyToCubemaps - Lighting");
		if(mVideoTexture->IsValid())
		{
			inputCommandCreateInfo.effectPassName = "ExtractTagDataID";
			scr::uvec3 size  = {1,1,1};
			scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, size, mExtractTagDataIDEffect, {mCubemapComputeShaderResources[0][2]});
			cubemapUB.faceSize			=tc.width;
			cubemapUB.sourceOffset		={(int32_t)mVideoTexture->GetTextureCreateInfo().width - (32 * 4), (int32_t)mVideoTexture->GetTextureCreateInfo().height - 4};
			mDeviceContext.DispatchCompute(&inputCommand);

			inputCommandCreateInfo.effectPassName = "ExtractOneTag";
			scr::InputCommand_Compute extractTagCommand(&inputCommandCreateInfo, size, mExtractOneTagEffect, {mCubemapComputeShaderResources[0][2]});
			mDeviceContext.DispatchCompute(&extractTagCommand);
		}
	}
	UpdateTagDataBuffers();
}

void ClientRenderer::UpdateTagDataBuffers()
{
	// TODO: too slow.
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto &cachedLights=resourceManagers->mLightManager.GetCache(cacheLock);
	if (lastSetupCommand.video_config.use_cubemap)
	{
		VideoTagDataCube *data=static_cast<VideoTagDataCube*>(mTagDataArrayBuffer->Map());
		if(data)
		{
			//VideoTagDataCube &data=*pdata;
			for (size_t i = 0; i < videoTagDataCubeArray.size(); ++i)
			{
				const auto& td = videoTagDataCubeArray[i];
				const auto& pos = td.coreData.cameraTransform.position;
				const auto& rot = td.coreData.cameraTransform.rotation;

				data[i].cameraPosition = { pos.x, pos.y, pos.z };
				data[i].cameraRotation = { rot.x, rot.y, rot.z, rot.w };
				data[i].lightCount=td.lights.size();
				for(size_t j=0;j<td.lights.size();j++)
				{
					LightTag &t=data[i].lightTags[j];
					const scr::LightTagData &l=td.lights[j];
					t.uid32=(unsigned)(((uint64_t)0xFFFFFFFF)&l.uid);
					t.colour=*((avs::vec4*)&l.color);
					// Convert from +-1 to [0,1]
					t.shadowTexCoordOffset.x=float(l.texturePosition[0])/float(lastSetupCommand.video_config.video_width);
					t.shadowTexCoordOffset.y=float(l.texturePosition[1])/float(lastSetupCommand.video_config.video_height);
					t.shadowTexCoordScale.x=float(l.textureSize)/float(lastSetupCommand.video_config.video_width);
					t.shadowTexCoordScale.y=float(l.textureSize)/float(lastSetupCommand.video_config.video_height);
					// Because tag data is NOT properly transformed in advance yet:
					avs::vec3 position		=l.position;
					avs::vec4 orientation	=l.orientation;
					avs::ConvertPosition(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, position);
					avs::ConvertRotation(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, orientation);
					t.position=*((avs::vec3*)&position);
					ovrQuatf q={orientation.x,orientation.y,orientation.z,orientation.w};
					avs::vec3 z={0,0,1.0f};
					t.direction=QuaternionTimesVector(q,z);
					scr::mat4 worldToShadowMatrix=scr::mat4((const float*)&l.worldToShadowMatrix);

					t.worldToShadowMatrix	=*((ovrMatrix4f*)&worldToShadowMatrix);

					const auto &nodeLight=cachedLights.find(l.uid);
					if(nodeLight!=cachedLights.end())
					{
						auto *lightData=nodeLight->second.resource->GetLightData();
						if(lightData)
						{
							t.colour=(const float*)(&lightData->colour);
							t.is_spot=lightData->is_spot;
							t.is_point=lightData->is_point;
							t.radius=lightData->radius;
							t.shadow_strength=0.0f;
						}
						else
						{
							OVR_WARN("No matching cached light.");
						}

					}
				}
			}
			mTagDataArrayBuffer->Unmap();
		}
		//mTagDataArrayBuffer->Update(32*sizeof(VideoTagDataCube),data,0);
	}
	else
	{
		VideoTagData2D data[MAX_TAG_DATA_COUNT];
		for (size_t i = 0; i < mVideoTagData2DArray.size(); ++i)
		{
			const auto& td = mVideoTagData2DArray[i];
			const auto& pos = td.cameraTransform.position;
			const auto& rot = td.cameraTransform.rotation;

			data[i].cameraPosition = { pos.x, pos.y, pos.z };
			data[i].cameraRotation = { rot.x, rot.y, rot.z, rot.w };
		}
		//tagData2DBuffer.SetData(deviceContext, data);
	}
}
void ClientRenderer::RenderVideo(scc::GL_DeviceContext &mDeviceContext,OVR::ovrFrameResult &res)
{
    {
        ovrMatrix4f eye0=res.FrameMatrices.EyeView[ 0 ];
        eye0.M[0][3]=0.0f;
        eye0.M[1][3]=0.0f;
        eye0.M[2][3]=0.0f;
        ovrMatrix4f viewProj0=res.FrameMatrices.EyeProjection[ 0 ]*eye0;
        ovrMatrix4f viewProjT0=ovrMatrix4f_Transpose( &viewProj0 );
        videoUB.invViewProj[0]=ovrMatrix4f_Inverse( &viewProjT0 );
        ovrMatrix4f eye1=res.FrameMatrices.EyeView[ 1 ];
        eye1.M[0][3]=0.0f;
        eye1.M[1][3]=0.0f;
        eye1.M[2][3]=0.0f;
        ovrMatrix4f viewProj1=res.FrameMatrices.EyeProjection[ 1 ]*eye1;
        ovrMatrix4f viewProjT1=ovrMatrix4f_Transpose( &viewProj1 );
        videoUB.invViewProj[1]=ovrMatrix4f_Inverse( &viewProjT1 );
    }
    // Set data to send to the shader:
    if(mCubemapTexture->IsValid())
    {
        ovrQuatf X0={1.0f,0.f,0.f,0.0f};
        ovrQuatf headPoseQ={headPose.orientation.x,headPose.orientation.y,headPose.orientation.z,headPose.orientation.w};
        ovrQuatf headPoseC={-headPose.orientation.x,-headPose.orientation.y,-headPose.orientation.z,headPose.orientation.w};
        ovrQuatf xDir= QuaternionMultiply(QuaternionMultiply(headPoseQ,X0),headPoseC);
        float w=eyeSeparation/2.0f;//.04f; //half separation.
        avs::vec4 eye={w*xDir.x,w*xDir.y,w*xDir.z,0.0f};
        avs::vec4 left_eye ={-eye.x,-eye.y,-eye.z,0.0f};
        videoUB.eyeOffsets[0]=left_eye;		// left eye
        videoUB.eyeOffsets[1]=eye;	// right eye.
        videoUB.cameraPosition=cameraPosition;

        mVideoSurfaceDef.graphicsCommand.UniformData[0].Data = &(((scc::GL_Texture *) mCubemapTexture.get())->GetGlTexture());
        //mVideoSurfaceDef.graphicsCommand.UniformData[3].Data = &(((scc::GL_Texture *)  mVideoTexture.get())->GetGlTexture());
		mVideoSurfaceDef.graphicsCommand.UniformData[1].Data =  &(((scc::GL_UniformBuffer *)  		mVideoUB.get())->GetGlBuffer());
		//mVideoSurfaceDef.graphicsCommand.UniformData[2].Data =  &(((scc::GL_ShaderStorageBuffer *)  mTagDataIDBuffer.get())->GetGlBuffer());
		OVR::GlBuffer& buf=((scc::GL_ShaderStorageBuffer *)  mTagDataBuffer.get())->GetGlBuffer();
		mVideoSurfaceDef.graphicsCommand.UniformData[2].Data =  &buf;
        res.Surfaces.push_back(ovrDrawSurface(&mVideoSurfaceDef));
    }
    mVideoUB->Submit();
}

void ClientRenderer::UpdateHandObjects()
{
	std::vector<ovrTracking> remoteStates;

	uint32_t deviceIndex = 0;
	ovrInputCapabilityHeader capsHeader;
	//Poll controller state from the Oculus API.
	while(vrapi_EnumerateInputDevices(mOvrMobile, deviceIndex, &capsHeader) >= 0)
	{
		if(capsHeader.Type == ovrControllerType_TrackedRemote)
		{
			ovrTracking remoteState;
			if(vrapi_GetInputTrackingState(mOvrMobile, capsHeader.DeviceID, 0, &remoteState) >= 0)
			{
				remoteStates.push_back(remoteState);
				if(deviceIndex < 2)
				{
					avs::vec3 pos = clientDeviceState->localOriginPos + *((const avs::vec3 *)&remoteState.HeadPose.Pose.Position);

					controllerPoses[deviceIndex].position = *((const avs::vec3 *)(&pos));
					controllerPoses[deviceIndex].orientation = *((const avs::vec4 *)(&remoteState.HeadPose.Pose.Orientation));
				}
				else
				{
					break;
				}
			}
		}
		++deviceIndex;
	}

	std::shared_ptr<scr::Node> leftHand, rightHand;
	resourceManagers->mNodeManager->GetHands(leftHand, rightHand);

	switch(remoteStates.size())
	{
		case 0:
			return;
		case 1: //Set non-dominant hand away. TODO: Query OVR for which hand is dominant/in-use.
			leftHand = nullptr;
			break;
		default:
			break;
	}

	if(rightHand)
	{
		rightHand->UpdateModelMatrix
				(
						avs::vec3
								{
										remoteStates[0].HeadPose.Pose.Position.x + cameraPosition.x,
										remoteStates[0].HeadPose.Pose.Position.y + cameraPosition.y,
										remoteStates[0].HeadPose.Pose.Position.z + cameraPosition.z
								},
						scr::quat
								{
										remoteStates[0].HeadPose.Pose.Orientation.x,
										remoteStates[0].HeadPose.Pose.Orientation.y,
										remoteStates[0].HeadPose.Pose.Orientation.z,
										remoteStates[0].HeadPose.Pose.Orientation.w
								}
						* HAND_ROTATION_DIFFERENCE,
						rightHand->GetGlobalTransform().m_Scale
				);
	}

	if(leftHand)
	{
		leftHand->UpdateModelMatrix
				(
						avs::vec3
								{
										remoteStates[1].HeadPose.Pose.Position.x + cameraPosition.x,
										remoteStates[1].HeadPose.Pose.Position.y + cameraPosition.y,
										remoteStates[1].HeadPose.Pose.Position.z + cameraPosition.z
								},
						scr::quat
								{
										remoteStates[1].HeadPose.Pose.Orientation.x,
										remoteStates[1].HeadPose.Pose.Orientation.y,
										remoteStates[1].HeadPose.Pose.Orientation.z,
										remoteStates[1].HeadPose.Pose.Orientation.w
								}
						* HAND_ROTATION_DIFFERENCE,
						leftHand->GetGlobalTransform().m_Scale
				);
	}
}

void ClientRenderer::RenderLocalActors(ovrFrameResult& res)
{
	//Render local actors.
	const scr::NodeManager::actorList_t &rootActors = resourceManagers->mNodeManager->GetRootActors();
	for(std::shared_ptr<scr::Node> actor : rootActors)
	{
		RenderActor(res, actor);
	}

	//Retrieve hands.
	std::shared_ptr<scr::Node> leftHand, rightHand;
	resourceManagers->mNodeManager->GetHands(leftHand, rightHand);

	//Render hands, if they exist.
	if(leftHand)
		RenderActor(res, leftHand);
	if(rightHand)
		RenderActor(res, rightHand);
}


void ClientRenderer::RenderActor(ovrFrameResult& res, std::shared_ptr<scr::Node> actor)
{
	std::shared_ptr<OVRActor> ovrActor = std::static_pointer_cast<OVRActor>(actor);

	//----OVR Node Set Transforms----//
	scr::mat4 globalMatrix=actor->GetGlobalTransform().GetTransformMatrix();
	clientDeviceState->transformToLocalOrigin=scr::mat4::Translation(-clientDeviceState->localOriginPos);
	scr::mat4 scr_Transform = clientDeviceState->transformToLocalOrigin * globalMatrix;

	std::shared_ptr<scr::Skin> skin = ovrActor->GetSkin();
	if(skin)
		skin->UpdateBoneMatrices(globalMatrix);

	OVR::Matrix4f transform;
	memcpy(&transform.M[0][0], &scr_Transform.a, 16 * sizeof(float));

	for(size_t matIndex = 0; matIndex < actor->GetMaterials().size(); matIndex++)
	{
		if(matIndex >= ovrActor->ovrSurfaceDefs.size())
		{
			//OVR_LOG("Skipping empty element in ovrSurfaceDefs.");
			break;
		}

		res.Surfaces.emplace_back(transform, &ovrActor->ovrSurfaceDefs[matIndex]);
	}

	for(std::weak_ptr<scr::Node> childPtr : actor->GetChildren())
	{
		std::shared_ptr<scr::Node> child = childPtr.lock();
		if(child)
		{
			RenderActor(res, child);
		}
	}
}

void ClientRenderer::ToggleTextures()
{
	OVRNodeManager* NodeManager = dynamic_cast<OVRNodeManager*>(resourceManagers->mNodeManager.get());
	passSelector++;
	passSelector=passSelector%(passNames.size());
	NodeManager->ChangeEffectPass(passNames[passSelector].c_str());
}


void ClientRenderer::ToggleShowInfo()
{
	show_osd = (show_osd + 1) % NUM_OSDS;
}

void  ClientRenderer::SetStickOffset(float x,float y)
{
	//ovrFrameInput *f = const_cast<ovrFrameInput *>(&vrFrame);
	//f->Input.sticks[0][0] += dx;
	//f->Input.sticks[0][1] += dy;
}

static float frameRate=1.0f;
void ClientRenderer::Render(const OVR::ovrFrameInput& vrFrame,OVR::OvrGuiSys *mGuiSys)
{
	if(vrFrame.DeltaSeconds>0.0f)
	{
		frameRate*=0.99f;
		frameRate+=0.01f/vrFrame.DeltaSeconds;
	}
	if(show_osd!=NO_OSD)
	{
		DrawOSD(mGuiSys);
	}
}

void ClientRenderer::DrawOSD(OVR::OvrGuiSys *mGuiSys)
{
	auto ctr=mNetworkSource.getCounterValues();
	if(show_osd== NETWORK_OSD)
	{
		mGuiSys->ShowInfoText(
				INFO_TEXT_DURATION,
				"Frames: %d\nPackets Dropped: Network %d | Decoder %d\n"
				"Incomplete Decoder Packets: %d\n"
				"Framerate: %4.4f Bandwidth(kbps): %4.4f"
				,mDecoder.getTotalFramesProcessed(), ctr.networkPacketsDropped,
				ctr.decoderPacketsDropped,
				ctr.incompleteDecoderPacketsReceived,
				frameRate, ctr.bandwidthKPS);
	}
	else if(show_osd== CAMERA_OSD)
	{
		mGuiSys->ShowInfoText(
				INFO_TEXT_DURATION,
				      "Clientspace  Origin: %1.3f, %1.3f, %1.3f\n"
					       "  + Camera Relative: %1.3f, %1.3f, %1.3f\n"
					       "  = Camera Position: %1.3f, %1.3f, %1.3f\n"
				,clientDeviceState->localOriginPos.x,clientDeviceState->localOriginPos.y,clientDeviceState->localOriginPos.z
				,clientDeviceState->relativeHeadPos.x,clientDeviceState->relativeHeadPos.y,clientDeviceState->relativeHeadPos.z
				,cameraPosition.x, cameraPosition.y, cameraPosition.z
				);
	}
	else if(show_osd == GEOMETRY_OSD)
	{
		mGuiSys->ShowInfoText
		(
			INFO_TEXT_DURATION,
		"%s\n"
			"Actors: %d \n"
			"Orphans: %d",
			GlobalGraphicsResources.effectPassName,
			static_cast<uint64_t>(resourceManagers->mNodeManager->GetActorAmount()),
			ctr.m_packetMapOrphans
		);

		const auto& missingResources = resourceCreator->GetMissingResources();
		if(missingResources.size() > 0)
		{
			std::ostringstream missingResourcesStream;
			missingResourcesStream << "Missing Resources\n";

			size_t resourcesOnLine = 0;
			for(const auto& missingPair : missingResources)
			{
				const ResourceCreator::MissingResource& missingResource = missingPair.second;
				missingResourcesStream << missingResource.resourceType << "_" << missingResource.id;

				resourcesOnLine++;
				if(resourcesOnLine >= MAX_RESOURCES_PER_LINE)
				{
					missingResourcesStream << std::endl;
					resourcesOnLine = 0;
				}
				else
				{
					missingResourcesStream << " | ";
				}
			}

			mGuiSys->ShowInfoText(INFO_TEXT_DURATION, missingResourcesStream.str().c_str());
		}
	}
	else if(show_osd==TAG_OSD)
	{
		std::ostringstream sstr;
		std::setprecision(5);
		sstr<<"Tags\n"<<std::setw(4) ;
		for(size_t i=0;i<std::min((size_t)3,videoTagDataCubeArray.size());i++)
		{
			auto &tag=videoTagDataCubeArray[i];
			sstr<<tag.coreData.lightCount<<" lights\n";
			for(size_t j=0;j<tag.lights.size();j++)
			{
				auto &l=tag.lights[j];
				sstr<<"\t"<<l.uid<<": Type "<<ToString((scr::Light::Type)l.lightType)<<", clr "<<l.color.x<<","<<l.color.y<<","<<l.color.z;
				if(l.lightType==scr::LightType::Directional)
				{
					ovrQuatf q={l.orientation.x,l.orientation.y,l.orientation.z,l.orientation.w};
					avs::vec3 z={0,0,1.0f};
					avs::vec3 direction=QuaternionTimesVector(q,z);
					sstr << ", d " << direction.x << "," << direction.y << "," << direction.z;
				}
				sstr<<"\n";
			}
		}
		mGuiSys->ShowInfoText(INFO_TEXT_DURATION,sstr.str().c_str());
	}
}