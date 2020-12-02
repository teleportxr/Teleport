//
// Created by roder on 06/04/2020.
//
#include "ClientRenderer.h"

#include <algorithm>
#include <sstream>

#include "OVR_GlUtils.h"
#include "OVR_Math.h"
#include <VrApi_Types.h>
#include <VrApi_Input.h>

#include "OVRActorManager.h"

using namespace OVR;
ClientRenderer::ClientRenderer(ResourceCreator *r,scr::ResourceManagers *rm,SessionCommandInterface *i,ClientAppInterface *c)
		:mDecoder(avs::DecoderBackend::Custom)
		, localOriginPos(0,0,0)
		,relativeHeadPos(0,0,0)
		, transformToOculusOrigin(scr::mat4::Translation(-localOriginPos))
		, resourceManagers(rm)
		, resourceCreator(r)
		, clientAppInterface(c)
		, mOvrMobile(nullptr)
        , mVideoSurfaceTexture(nullptr)
        , mCubemapTexture(nullptr)
        , mCubemapLightingTexture(nullptr)
        , mTagDataIDBuffer(nullptr)
		, mTagDataBuffer(nullptr)
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
		static ovrProgramParm uniformParms[]  =    // both TextureMvpProgram and CubeMapPanoProgram use the same parm mapping
				                      {
						                        {"colourOffsetScale", ovrProgramParmType::FLOAT_VECTOR4}
						                      , {"depthOffsetScale" , ovrProgramParmType::FLOAT_VECTOR4}
						                      , {"cubemapTexture"   , ovrProgramParmType::TEXTURE_SAMPLED}
						                      , {"videoFrameTexture", ovrProgramParmType::TEXTURE_SAMPLED}
						                      , {"videoUB"          , ovrProgramParmType::BUFFER_UNIFORM}
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
		mDiffuseTexture      =GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mSpecularTexture     =GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mRoughSpecularTexture=GlobalGraphicsResources.renderPlatform.InstantiateTexture();

		mCubemapLightingTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mTagDataIDBuffer = GlobalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
		mTagDataBuffer = GlobalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
		mTagDataBuffer = GlobalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
	}
	// Tag Data ID
	{
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo shaderStorageBufferCreateInfo = {
				3,
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
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo shaderStorageBufferCreateInfo = {
				4,
				scr::ShaderStorageBuffer::Access::NONE,
				sizeof(VideoTagDataCube) * MAX_TAG_DATA_COUNT,
				(void*)&shaderTagDataCubeArray
		};
		mTagDataBuffer->Create(&shaderStorageBufferCreateInfo);
	}
	else
	{
		// Tag Data 2D Buffer
		VideoTagData2D shaderTagData2DArray[MAX_TAG_DATA_COUNT];
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo shaderStorageBufferCreateInfo = {
				4,
				scr::ShaderStorageBuffer::Access::NONE,
				sizeof(VideoTagData2D) * MAX_TAG_DATA_COUNT,
				(void*)&shaderTagData2DArray
		};
		mTagDataBuffer->Create(&shaderStorageBufferCreateInfo);

	}

	{
		CopyCubemapSrc     = clientAppInterface->LoadTextFile("shaders/CopyCubemap.comp");
		mCopyCubemapEffect = GlobalGraphicsResources.renderPlatform.InstantiateEffect();
		mCopyCubemapWithDepthEffect = GlobalGraphicsResources.renderPlatform.InstantiateEffect();
		mExtractTagDataIDEffect=GlobalGraphicsResources.renderPlatform.InstantiateEffect();

		scr::Effect::EffectCreateInfo effectCreateInfo = {};
		effectCreateInfo.effectName = "CopyCubemap";
		mCopyCubemapEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "CopyCubemapWithDepth";
		mCopyCubemapWithDepthEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "ExtractTagDataID";
		mExtractTagDataIDEffect->Create(&effectCreateInfo);

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
		sr.AddBuffer(2, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 3, "TagDataID", {mTagDataIDBuffer.get()});

		mCubemapComputeShaderResources.push_back(sr);

		mCopyCubemapEffect->LinkShaders("CopyCubemap", {});
		mCopyCubemapWithDepthEffect->LinkShaders("ColourAndDepth",{});
		mExtractTagDataIDEffect->LinkShaders("ExtractTagDataID",{});
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
	vertLayout.AddBinding(4, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_VERTEX);
	vertLayout.AddBinding(3, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_VERTEX);
	scr::ShaderResourceLayout fragLayout;
	fragLayout.AddBinding(3, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
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
	pbrShaderResource.AddBuffer(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 4, "u_BoneData", {});
	pbrShaderResource.AddBuffer(1, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 3, "u_MaterialData", {});
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
		if(mDiffuseTexture->IsValid())
		{
			static uint32_t face= 0;
			mip_y = 0;
			int32_t mip_size=videoConfig.diffuse_cubemap_size;
			uint32_t M=mDiffuseTexture->GetTextureCreateInfo().mipCount;
			scr::ivec2 offset={offset0.x+diffuseOffset.x,offset0.y+diffuseOffset.y};
			for (uint32_t m        = 0; m < M; m++)
			{
				inputCommand.m_WorkGroupSize = {(mip_size + 1) / ThreadCount, (mip_size + 1) / ThreadCount ,6};
				mCubemapComputeShaderResources[0].SetImageInfo(1, 0, {mDiffuseTexture->GetSampler(), mDiffuseTexture, m});
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
		if(mSpecularTexture->IsValid())
		{
			mip_y = 0;
			int32_t          mip_size = videoConfig.specular_cubemap_size;
			uint32_t         M        = mSpecularTexture->GetTextureCreateInfo().mipCount;
			scr::ivec2       offset   = {
					offset0.x + specularOffset.x, offset0.y + specularOffset.y};
			for (uint32_t m        = 0; m < M; m++)
			{
				inputCommand.m_WorkGroupSize = {
						(mip_size + 1) / ThreadCount, (mip_size + 1) / ThreadCount, 6};
				mCubemapComputeShaderResources[0].SetImageInfo(
						1, 0, {mSpecularTexture->GetSampler(), mSpecularTexture, m});
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
			uint32_t         M        = mSpecularTexture->GetTextureCreateInfo().mipCount;
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
		}

	}
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
					avs::vec3 pos = localOriginPos + *((const avs::vec3 *)&remoteState.HeadPose.Pose.Position);

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
	resourceManagers->mActorManager->GetHands(leftHand, rightHand);

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
	const scr::ActorManager::actorList_t &rootActors = resourceManagers->mActorManager->GetRootActors();
	for(std::shared_ptr<scr::Node> actor : rootActors)
	{
		RenderActor(res, actor);
	}

	//Retrieve hands.
	std::shared_ptr<scr::Node> leftHand, rightHand;
	resourceManagers->mActorManager->GetHands(leftHand, rightHand);

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
	transformToOculusOrigin=scr::mat4::Translation(-localOriginPos);
	scr::mat4 scr_Transform = transformToOculusOrigin * globalMatrix;

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
	OVRActorManager* actorManager = dynamic_cast<OVRActorManager*>(resourceManagers->mActorManager.get());
	passSelector++;
	passSelector=passSelector%(passNames.size());
	actorManager->ChangeEffectPass(passNames[passSelector].c_str());
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
				"Framerate: %4.4f Bandwidth(kbps): %4.4f\n"
				"Actors: %d \n"
				"Orphans: %d\n",
				mDecoder.getTotalFramesProcessed(), ctr.networkPacketsDropped,
				ctr.decoderPacketsDropped,
				ctr.incompleteDecoderPacketsReceived,
				frameRate, ctr.bandwidthKPS,
				static_cast<uint64_t>(resourceManagers->mActorManager->GetActorAmount()),
				ctr.m_packetMapOrphans);
	}
	else if(show_osd== CAMERA_OSD)
	{
		mGuiSys->ShowInfoText(
				INFO_TEXT_DURATION,
				"  Camera Position: %1.3f, %1.3f, %1.3f\n"
				      "Received  Origin: %1.3f, %1.3f, %1.3f\n"
				,cameraPosition.x, cameraPosition.y, cameraPosition.z
				,localOriginPos.x,localOriginPos.y,localOriginPos.z
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
			static_cast<uint64_t>(resourceManagers->mActorManager->GetActorAmount()),
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
}