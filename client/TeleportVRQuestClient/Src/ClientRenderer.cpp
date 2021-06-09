//
// Created by roder on 06/04/2020.
//
#include "ClientRenderer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <GLES3/gl32.h>

#include "OVR_Math.h"
#include <VrApi_Types.h>
#include <VrApi_Input.h>
#include <VrApi_Helpers.h>

#include "crossplatform/ServerTimestamp.h"

#include "ClientDeviceState.h"
#include "OVRNode.h"
#include "OVRNodeManager.h"
#include "Config.h"

using namespace OVR;
using namespace OVRFW;
using namespace teleport;
using namespace client;

ovrQuatf QuaternionMultiply(const ovrQuatf &p, const ovrQuatf &q)
{
	ovrQuatf r;
	r.w = p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
	r.x = p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
	r.y = p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z;
	r.z = p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x;
	return r;
}
static const char *ToString(scr::Light::Type type)
{
	const char *lightTypeName = "";
	switch (type)
	{
		case scr::Light::Type::POINT:
			lightTypeName = "Point";
			break;
		case scr::Light::Type::DIRECTIONAL:
			lightTypeName = "  Dir";
			break;
		case scr::Light::Type::SPOT:
			lightTypeName = " Spot";
			break;
		case scr::Light::Type::AREA:
			lightTypeName = " Area";
			break;
		case scr::Light::Type::DISC:
			lightTypeName = " Disc";
			break;
		default:
			lightTypeName = "UNKNOWN";
			break;
	};
	return lightTypeName;
}

avs::vec3 QuaternionTimesVector(const ovrQuatf &q, const avs::vec3 &vec)
{
	const float &x0 = vec.x;
	const float &y0 = vec.y;
	const float &z0 = vec.z;
	float s1 = q.x * x0 + q.y * y0 + q.z * z0;
	float x1 = q.w * x0 + q.y * z0 - q.z * y0;
	float y1 = q.w * y0 + q.z * x0 - q.x * z0;
	float z1 = q.w * z0 + q.x * y0 - q.y * x0;
	avs::vec3 ret = {s1 * q.x + q.w * x1 + q.y * z1 - q.z * y1, s1 * q.y + q.w * y1 + q.z * x1
																- q.x * z1, s1 * q.z + q.w * z1
																			+ q.x * y1 - q.y * x1};
	return ret;
}

ClientRenderer::ClientRenderer
(
    ResourceCreator *r,
    scr::ResourceManagers *rm,
    ClientAppInterface *c,
    teleport::client::ClientDeviceState *s,
    Controllers *cn
)
    :controllers(cn),
     mDecoder(avs::DecoderBackend::Custom),
     resourceManagers(rm),
     resourceCreator(r),
     clientAppInterface(c),
     clientDeviceState(s)
{
}

ClientRenderer::~ClientRenderer()
{
	ExitedVR();
}

void ClientRenderer::EnteredVR(const ovrJava *java)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	//VideoSurfaceProgram
	{
		{
			mVideoUB = globalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
			scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo = {1
					, sizeof(VideoUB)
					, &videoUB};
			mVideoUB->Create(&uniformBufferCreateInfo);
		}
		static ovrProgramParm uniformParms[] =
				{
						  {"renderTexture", ovrProgramParmType::TEXTURE_SAMPLED}
						, {"videoUB"       , ovrProgramParmType::BUFFER_UNIFORM}
						, {"TagDataCube"   , ovrProgramParmType::BUFFER_STORAGE}
						,};

		// Cubemap
		{
			std::string videoSurfaceVert = clientAppInterface->LoadTextFile(
					"shaders/VideoSurfaceSphere.vert");
			std::string videoSurfaceFrag = clientAppInterface->LoadTextFile(
					"shaders/VideoSurfaceSphere.frag");
			mCubeVideoSurfaceProgram = GlProgram::Build(
					nullptr, videoSurfaceVert.c_str(),
					"#extension GL_OES_EGL_image_external_essl3 : require\n",
					videoSurfaceFrag.c_str(),
					uniformParms, sizeof(uniformParms) / sizeof(ovrProgramParm), 310);
			if (!mCubeVideoSurfaceProgram.IsValid()) {
				OVR_FAIL("Failed to build video surface shader program for cubemap rendering");
			}
		}
		// Perspective
		{
			std::string videoSurfaceVert = clientAppInterface->LoadTextFile(
					"shaders/VideoSurfaceSpherePersp.vert");
			std::string videoSurfaceFrag = clientAppInterface->LoadTextFile(
					"shaders/VideoSurfaceSpherePersp.frag");
			m2DVideoSurfaceProgram = GlProgram::Build(
					nullptr, videoSurfaceVert.c_str(),
					"#extension GL_OES_EGL_image_external_essl3 : require\n",
					videoSurfaceFrag.c_str(),
					uniformParms, sizeof(uniformParms) / sizeof(ovrProgramParm), 310);
			if (!m2DVideoSurfaceProgram.IsValid()) {
				OVR_FAIL("Failed to build video surface shader program for perspective rendering");
			}
		}
	}
	{
		mVideoSurfaceTexture = new OVRFW::SurfaceTexture(java->Env);
		mVideoTexture = globalGraphicsResources.renderPlatform.InstantiateTexture();
		mCubemapUB = globalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
		mRenderTexture = globalGraphicsResources.renderPlatform.InstantiateTexture();
		diffuseCubemapTexture = globalGraphicsResources.renderPlatform.InstantiateTexture();
		specularCubemapTexture = globalGraphicsResources.renderPlatform.InstantiateTexture();

		mCubemapLightingTexture = globalGraphicsResources.renderPlatform.InstantiateTexture();
		mTagDataIDBuffer = globalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
		mTagDataArrayBuffer = globalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
	}
	// Tag Data ID
	{
		scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo shaderStorageBufferCreateInfo = {
				0, scr::ShaderStorageBuffer::Access::NONE, sizeof(scr::uvec4), (void *) &mTagDataID
		};
		mTagDataIDBuffer->Create(&shaderStorageBufferCreateInfo);
	}


	// Tag Data Cube Buffer
	VideoTagDataCube shaderTagDataCubeArray[MAX_TAG_DATA_COUNT];
	shaderTagDataCubeArray[0].cameraPosition.x = 1.0f;
	scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo tagBufferCreateInfo = {
			1, scr::ShaderStorageBuffer::Access::READ_WRITE_BIT, sizeof(VideoTagDataCube)
			, (void *) nullptr
	};
	globalGraphicsResources.mTagDataBuffer->Create(&tagBufferCreateInfo);

	scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo arrayBufferCreateInfo = {
			2, scr::ShaderStorageBuffer::Access::READ_WRITE_BIT, sizeof(VideoTagDataCube)
																 * MAX_TAG_DATA_COUNT
			, (void *) &shaderTagDataCubeArray
	};
	mTagDataArrayBuffer->Create(&arrayBufferCreateInfo);


	{
		CopyCubemapSrc = clientAppInterface->LoadTextFile("shaders/CopyCubemap.comp");
		mCopyCubemapEffect = globalGraphicsResources.renderPlatform.InstantiateEffect();
		mCopyCubemapWithDepthEffect = globalGraphicsResources.renderPlatform.InstantiateEffect();
		mCopyPerspectiveEffect = globalGraphicsResources.renderPlatform.InstantiateEffect();
		mExtractTagDataIDEffect = globalGraphicsResources.renderPlatform.InstantiateEffect();
		mExtractOneTagEffect = globalGraphicsResources.renderPlatform.InstantiateEffect();
		scr::Effect::EffectCreateInfo effectCreateInfo = {};
		effectCreateInfo.effectName = "CopyCubemap";
		mCopyCubemapEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "CopyCubemapWithDepth";
		mCopyCubemapWithDepthEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "CopyPerspective";
		mCopyPerspectiveEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "ExtractTagDataID";
		mExtractTagDataIDEffect->Create(&effectCreateInfo);

		effectCreateInfo.effectName = "ExtractOneTag";
		mExtractOneTagEffect->Create(&effectCreateInfo);

		scr::ShaderSystem::PipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.m_Count = 1;
		pipelineCreateInfo.m_PipelineType = scr::ShaderSystem::PipelineType::PIPELINE_TYPE_COMPUTE;
		pipelineCreateInfo.m_ShaderCreateInfo[0].stage = scr::Shader::Stage::SHADER_STAGE_COMPUTE;
		pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "colour_only";
		pipelineCreateInfo.m_ShaderCreateInfo[0].filepath = "shaders/CopyCubemap.comp";
		pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = CopyCubemapSrc;
		scr::ShaderSystem::Pipeline cp(&globalGraphicsResources.renderPlatform,
									   &pipelineCreateInfo);

		scr::Effect::EffectPassCreateInfo effectPassCreateInfo;
		effectPassCreateInfo.effectPassName = "CopyCubemap";
		effectPassCreateInfo.pipeline = cp;
		mCopyCubemapEffect->CreatePass(&effectPassCreateInfo);

		pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "colour_and_depth";
		scr::ShaderSystem::Pipeline cp2(&globalGraphicsResources.renderPlatform,
										&pipelineCreateInfo);

		effectPassCreateInfo.effectPassName = "ColourAndDepth";
		effectPassCreateInfo.pipeline = cp2;
		mCopyCubemapWithDepthEffect->CreatePass(&effectPassCreateInfo);

		{
			std::string copyPerspectiveSrc = clientAppInterface->LoadTextFile(
					"shaders/CopyPerspective.comp");
			// pass to extract from the array into a single tag buffer:
			pipelineCreateInfo.m_ShaderCreateInfo[0].filepath = "shaders/CopyPerspective.comp";
			pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = copyPerspectiveSrc;
			pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "colour_and_depth";
			scr::ShaderSystem::Pipeline cp3(&globalGraphicsResources.renderPlatform,
											&pipelineCreateInfo);
			effectPassCreateInfo.effectPassName = "PerspectiveColourAndDepth";
			effectPassCreateInfo.pipeline = cp3;
			mCopyPerspectiveEffect->CreatePass(&effectPassCreateInfo);
		}

		{
			ExtractTagDataIDSrc = clientAppInterface->LoadTextFile("shaders/ExtractTagDataID.comp");
			pipelineCreateInfo.m_ShaderCreateInfo[0].filepath = "shaders/ExtractTagDataID.comp";
			pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = ExtractTagDataIDSrc;
			pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "extract_tag_data_id";
			scr::ShaderSystem::Pipeline cp4(&globalGraphicsResources.renderPlatform,
											&pipelineCreateInfo);
			effectPassCreateInfo.effectPassName = "ExtractTagDataID";
			effectPassCreateInfo.pipeline = cp4;
			mExtractTagDataIDEffect->CreatePass(&effectPassCreateInfo);

			std::string ExtractTagDataSrc = clientAppInterface->LoadTextFile(
					"shaders/ExtractOneTag.comp");
			// pass to extract from the array into a single tag buffer:
			pipelineCreateInfo.m_ShaderCreateInfo[0].filepath = "shaders/ExtractOneTag.comp";
			pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = ExtractTagDataSrc;
			pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "extract_tag_data";
			scr::ShaderSystem::Pipeline cp5(&globalGraphicsResources.renderPlatform,
											&pipelineCreateInfo);
			effectPassCreateInfo.effectPassName = "ExtractOneTag";
			effectPassCreateInfo.pipeline = cp5;
			mExtractOneTagEffect->CreatePass(&effectPassCreateInfo);

			scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo = {2
					, sizeof(CubemapUB)
					, &cubemapUB};
			mCubemapUB->Create(&uniformBufferCreateInfo);
		}
		GLCheckErrorsWithTitle("mCubemapUB:Create");

		scr::ShaderResourceLayout layout;
		layout.AddBinding(0, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE,
						  scr::Shader::Stage::SHADER_STAGE_COMPUTE);
		layout.AddBinding(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,
						  scr::Shader::Stage::SHADER_STAGE_COMPUTE);
		layout.AddBinding(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,
						  scr::Shader::Stage::SHADER_STAGE_COMPUTE);
		layout.AddBinding(3, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER,
						  scr::Shader::Stage::SHADER_STAGE_COMPUTE);

		mColourAndDepthShaderResources.SetLayout(layout);
		mColourAndDepthShaderResources.AddImage(
				scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, 0, "destTex",
				{globalGraphicsResources.cubeMipMapSampler, mRenderTexture, 0, uint32_t(-1)});
		mColourAndDepthShaderResources.AddImage(
				scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1,
				"videoFrameTexture", {globalGraphicsResources.sampler, mVideoTexture});
		mColourAndDepthShaderResources.AddBuffer(
				scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB",
				{mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});

		mCopyCubemapShaderResources.SetLayout(layout);
		mCopyCubemapShaderResources.AddImage(
				scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, 0,
				"destTex", {});
		mCopyCubemapShaderResources.AddImage(
				scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1,
				"videoFrameTexture", {globalGraphicsResources.sampler, mVideoTexture});
		mCopyCubemapShaderResources.AddBuffer(
				scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB",
				{mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});

		mCopyPerspectiveShaderResources.SetLayout(layout);
		mCopyPerspectiveShaderResources.AddImage(
				scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, 0,
				"destTex", {globalGraphicsResources.sampler, mRenderTexture});
		mCopyPerspectiveShaderResources.AddImage(
				scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1,
				"videoFrameTexture", {globalGraphicsResources.sampler, mVideoTexture});
		mCopyPerspectiveShaderResources.AddBuffer(
				scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB",
				{mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});

		mExtractTagShaderResources.SetLayout(layout);
		mExtractTagShaderResources.AddImage(
				scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1,
				"videoFrameTexture", {globalGraphicsResources.sampler, mVideoTexture});
		mExtractTagShaderResources.AddBuffer(
				scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB",
				{mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});
		mExtractTagShaderResources.AddBuffer(
				scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 0, "TagDataID",
				{mTagDataIDBuffer.get()});
		mExtractTagShaderResources.AddBuffer(
				scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 1,
				"TagDataCube_ssbo", {globalGraphicsResources.mTagDataBuffer.get()});
		mExtractTagShaderResources.AddBuffer(
				scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 2,
				"TagDataCubeArray_ssbo", {mTagDataArrayBuffer.get()});


		mCopyCubemapWithDepthEffect->LinkShaders("ColourAndDepth",{mColourAndDepthShaderResources});
		mCopyCubemapEffect->LinkShaders("CopyCubemap", {mCopyCubemapShaderResources});
		mCopyPerspectiveEffect->LinkShaders("PerspectiveColourAndDepth",{mCopyPerspectiveShaderResources});
		mExtractTagDataIDEffect->LinkShaders("ExtractTagDataID", {mExtractTagShaderResources});
		mExtractOneTagEffect->LinkShaders("ExtractOneTag", {mExtractTagShaderResources});
	}

	mVideoSurfaceDef.surfaceName = "VideoSurface";
	mVideoSurfaceDef.geo = BuildGlobe(1.f, 1.f, 6.f,32,64);
	mVideoSurfaceDef.graphicsCommand.GpuState.depthEnable = true;
	mVideoSurfaceDef.graphicsCommand.GpuState.depthMaskEnable = false;
	mVideoSurfaceDef.graphicsCommand.GpuState.cullEnable = false;
	mVideoSurfaceDef.graphicsCommand.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_DISABLE;

	mWebcamResources.Init(clientAppInterface);

	//Set up scr::Camera
	scr::Camera::CameraCreateInfo c_ci = {
			(scr::RenderPlatform *) (&globalGraphicsResources.renderPlatform)
			,scr::Camera::ProjectionType::PERSPECTIVE, scr::quat(0.0f, 0.0f, 0.0f, 1.0f)
			,clientDeviceState->headPose.position, 5.0f
	};
	globalGraphicsResources.scrCamera = std::make_shared<scr::Camera>(&c_ci);

	scr::VertexBufferLayout layout;
	layout.AddAttribute(0, scr::VertexBufferLayout::ComponentCount::VEC3,scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(1, scr::VertexBufferLayout::ComponentCount::VEC3,scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(2, scr::VertexBufferLayout::ComponentCount::VEC4,scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(3, scr::VertexBufferLayout::ComponentCount::VEC2,scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(4, scr::VertexBufferLayout::ComponentCount::VEC2,scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(5, scr::VertexBufferLayout::ComponentCount::VEC4,scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(6, scr::VertexBufferLayout::ComponentCount::VEC4,scr::VertexBufferLayout::Type::FLOAT);
	layout.AddAttribute(7, scr::VertexBufferLayout::ComponentCount::VEC4,scr::VertexBufferLayout::Type::FLOAT);
	layout.CalculateStride();

	scr::ShaderResourceLayout shaderResourceLayout;
	shaderResourceLayout.AddBinding(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,scr::Shader::Stage::SHADER_STAGE_VERTEX);
	shaderResourceLayout.AddBinding(1, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER,scr::Shader::Stage::SHADER_STAGE_VERTEX);
	shaderResourceLayout.AddBinding(4, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,scr::Shader::Stage::SHADER_STAGE_VERTEX);
	shaderResourceLayout.AddBinding(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,scr::Shader::Stage::SHADER_STAGE_VERTEX);

	shaderResourceLayout.AddBinding(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(10, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(11, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(12, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(13, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(14, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(15, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(16, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	shaderResourceLayout.AddBinding(17, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,scr::Shader::Stage::SHADER_STAGE_FRAGMENT);

	scr::ShaderResource pbrShaderResource(shaderResourceLayout);
	pbrShaderResource.AddBuffer(scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,0,"u_CameraData", {});
	pbrShaderResource.AddBuffer(scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER,1,"TagDataID", {});
	pbrShaderResource.AddBuffer(scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,4,"u_BoneData", {});
	pbrShaderResource.AddBuffer(scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,2,"u_MaterialData", {});
	pbrShaderResource.AddImage(	scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,10,"u_DiffuseTexture", {});
	pbrShaderResource.AddImage(	scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,11,"u_NormalTexture", {});
	pbrShaderResource.AddImage(	scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,12,"u_CombinedTexture", {});
	pbrShaderResource.AddImage(	scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,13,"u_EmissiveTexture", {});
	pbrShaderResource.AddImage(	scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,14,"u_SpecularCubemap", {});
	//pbrShaderResource.AddImage(	scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,16,"u_DiffuseCubemap", {});

	passNames.clear();
	passNames.push_back("OpaquePBR");
	passNames.push_back("OpaqueAlbedo");
	passNames.push_back("OpaqueNormal");
	passNames.push_back("OpaquePBRAmbient");
	passNames.push_back("OpaquePBRDebug");

	scr::ShaderSystem::PipelineCreateInfo pipelineCreateInfo;

	pipelineCreateInfo.m_Count = 2;
	pipelineCreateInfo.m_PipelineType = scr::ShaderSystem::PipelineType::PIPELINE_TYPE_GRAPHICS;
	pipelineCreateInfo.m_ShaderCreateInfo[0].stage = scr::Shader::Stage::SHADER_STAGE_VERTEX;
	pipelineCreateInfo.m_ShaderCreateInfo[0].filepath = "shaders/OpaquePBR.vert";
	pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = clientAppInterface->LoadTextFile(
			"shaders/OpaquePBR.vert");
	pipelineCreateInfo.m_ShaderCreateInfo[1].stage = scr::Shader::Stage::SHADER_STAGE_FRAGMENT;
	pipelineCreateInfo.m_ShaderCreateInfo[1].filepath = "shaders/OpaquePBR.frag";
	pipelineCreateInfo.m_ShaderCreateInfo[1].sourceCode = clientAppInterface->LoadTextFile(
				"shaders/OpaquePBR.frag");
	std::string &src=pipelineCreateInfo.m_ShaderCreateInfo[1].sourceCode;
	// Now we will GENERATE the variants of the fragment shader, while adding them to the passNames list:
	for(int emissive=0;emissive<2;emissive++)
	{
		for(int combined=0;combined<2;combined++)
		{
			for(int normal=0;normal<2;normal++)
			{
				for(int diffuse=0;diffuse<2;diffuse++)
				{
					std::string passname=GlobalGraphicsResources::GenerateShaderPassName(diffuse,normal,combined,emissive);
					passNames.push_back(passname.c_str());
					static char txt2[2000];
					char const* true_or_false[]={"false","true"};
					sprintf(txt2,"\nvoid %s()\n{\nPBR(%s,%s,%s,%s,true, %d, false);\n}",
			 			passname.c_str(),true_or_false[diffuse], true_or_false[normal], true_or_false[combined], true_or_false[emissive],TELEPORT_MAX_LIGHTS);
					src+=txt2;
				}
			}
		}
	}
	//Static passes.
	pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "Static";
	for(const std::string& passName : passNames)
	{
		std::string completeName = "Static_" + passName;
		pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = passName.c_str();
		clientAppInterface->BuildEffectPass(completeName.c_str(), &layout, &pipelineCreateInfo,{pbrShaderResource});
	}
	//Skinned passes.
	pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "Animated";
	for(const std::string& passName : passNames)
	{
		std::string effectName = "Animated_" + passName;
		pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = passName.c_str();
		clientAppInterface->BuildEffectPass(effectName.c_str(), &layout, &pipelineCreateInfo,{pbrShaderResource});
	}
}

void ClientRenderer::WebcamResources::Init(ClientAppInterface* clientAppInterface)
{
	if (initialized)
	{
		return;
	}
	static ovrProgramParm wcUniformParms[] =
			{
					{"videoTexture", ovrProgramParmType::TEXTURE_SAMPLED},
					{"webcamUB",       ovrProgramParmType::BUFFER_UNIFORM}
			};
	std::string videoSurfaceVert = clientAppInterface->LoadTextFile(
			"shaders/Webcam.vert");
	std::string videoSurfaceFrag = clientAppInterface->LoadTextFile(
			"shaders/Webcam.frag");
	program = GlProgram::Build(
			nullptr, videoSurfaceVert.c_str(),
			"#extension GL_OES_EGL_image_external_essl3 : require\n",
			videoSurfaceFrag.c_str(),
			wcUniformParms, sizeof(wcUniformParms) / sizeof(ovrProgramParm), 310);
	if (!program.IsValid()) {
		OVR_FAIL("Failed to build video surface shader program for rendering webcam texture");
	}

	surfaceDef.surfaceName = "WebcamSurface";
	surfaceDef.graphicsCommand.GpuState.depthEnable = false;
	surfaceDef.graphicsCommand.GpuState.depthMaskEnable = false;
	surfaceDef.graphicsCommand.GpuState.cullEnable = false;
	surfaceDef.graphicsCommand.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_DISABLE;
	surfaceDef.graphicsCommand.GpuState.frontFace = GL_CW;
	surfaceDef.graphicsCommand.GpuState.polygonMode = GL_FILL;
	surfaceDef.graphicsCommand.Program = program;

	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();

	// Create vertex layout and associated buffers
	std::shared_ptr<scr::VertexBufferLayout> layout(new scr::VertexBufferLayout);
	layout->AddAttribute((uint32_t)avs::AttributeSemantic::POSITION, scr::VertexBufferLayout::ComponentCount::VEC3, scr::VertexBufferLayout::Type::FLOAT);
	layout->CalculateStride();
	layout->m_PackingStyle = scr::VertexBufferLayout::PackingStyle::INTERLEAVED;

	static constexpr size_t camVertexCount = 4;
	static constexpr size_t camIndexCount = 6;
	static constexpr avs::vec3 vertices[camVertexCount] = {{-1, -1, 0}, {-1, 1, 0}, {1, 1, 0}, {1, -1, 0}};
	static constexpr uint16_t indices[camIndexCount] = {0, 1, 3, 1, 2, 3}; // Clockwise

	size_t constructedVBSize = layout->m_Stride * camVertexCount;

	vertexBuffer = globalGraphicsResources.renderPlatform.InstantiateVertexBuffer();
	scr::VertexBuffer::VertexBufferCreateInfo vb_ci;
	vb_ci.layout = layout;
	vb_ci.usage = (scr::BufferUsageBit)(scr::STATIC_BIT | scr::DRAW_BIT);
	vb_ci.vertexCount = camVertexCount;
	vb_ci.size = constructedVBSize;
	vb_ci.data = (const void*)vertices;
	vertexBuffer->Create(&vb_ci);

	indexBuffer = globalGraphicsResources.renderPlatform.InstantiateIndexBuffer();
	scr::IndexBuffer::IndexBufferCreateInfo ib_ci;
	ib_ci.usage = (scr::BufferUsageBit)(scr::STATIC_BIT | scr::DRAW_BIT);
	ib_ci.indexCount = camIndexCount;
	ib_ci.stride = sizeof(uint16_t);
	ib_ci.data = (const uint8_t*)indices;
	indexBuffer->Create(&ib_ci);

	std::shared_ptr<scc::GL_VertexBuffer> gl_vb = std::dynamic_pointer_cast<scc::GL_VertexBuffer>(vertexBuffer);
	std::shared_ptr<scc::GL_IndexBuffer> gl_ib = std::dynamic_pointer_cast<scc::GL_IndexBuffer>(indexBuffer);

	gl_vb->CreateVAO(gl_ib->GetIndexID());

	// Create the GlGeometry for OVR and reference the GL buffers
	GlGeometry& geo = surfaceDef.geo;
	geo.vertexBuffer = gl_vb->GetVertexID();
	geo.indexBuffer = gl_ib->GetIndexID();
	geo.vertexArrayObject = gl_vb->GetVertexArrayID();
	geo.primitiveType = GL_TRIANGLES;
	geo.vertexCount = (int)gl_vb->GetVertexCount();
	geo.indexCount = (int)gl_ib->GetIndexBufferCreateInfo().indexCount;

	// Set up the uniform buffer
	webcamUB = globalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
	scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo = {1
			, sizeof(WebcamUB)
			, &webcamUBData};
	webcamUB->Create(&uniformBufferCreateInfo);

	// Bottom right corner
	const avs::vec2 pos =  { 1.0f - (WEBCAM_WIDTH * 0.5f), -1.0f + (WEBCAM_HEIGHT * 0.5f) };
	SetPosition(pos);

	initialized = true;
}

void ClientRenderer::WebcamResources::SetPosition(const avs::vec2& position)
{
	ovrMatrix4f translation = ovrMatrix4f_CreateTranslation(position.x, position.y, 0);

	// Width and height of original quad is 2 so scale will be half the width/height
	const avs::vec2 s = { WEBCAM_WIDTH * 0.5f, WEBCAM_HEIGHT * 0.5f };
	ovrMatrix4f scale = ovrMatrix4f_CreateScale(s.x, s.y, 1);

	auto t = ovrMatrix4f_Multiply(&translation, &scale);
	transform = ovrMatrix4f_Transpose(&t);
}

void ClientRenderer::WebcamResources::Destroy()
{
	if (initialized)
	{
		surfaceDef.geo.Free();
		GlProgram::Free(program);
		vertexBuffer->Destroy();
		indexBuffer->Destroy();
		webcamUB->Destroy();
		initialized = false;
	}
}

void ClientRenderer::SetWebcamPosition(const avs::vec2& position)
{
	mWebcamResources.SetPosition(position);
}

void ClientRenderer::RenderWebcam(OVRFW::ovrRendererOutput& res)
{
	// Set data to send to the shader:
	if (videoConfig.stream_webcam && mVideoTexture->IsValid() && mWebcamResources.initialized)
	{
		mWebcamResources.surfaceDef.graphicsCommand.UniformData[0].Data = &(((scc::GL_Texture *) mVideoTexture.get())->GetGlTexture());
		mWebcamResources.surfaceDef.graphicsCommand.UniformData[1].Data = &(((scc::GL_UniformBuffer *) mWebcamResources.webcamUB.get())->GetGlBuffer());
		res.Surfaces.emplace_back(mWebcamResources.transform, &mWebcamResources.surfaceDef);
		mWebcamResources.webcamUB->Submit();
	}
}

void ClientRenderer::ExitedVR()
{
	delete mVideoSurfaceTexture;
	mVideoSurfaceDef.geo.Free();
	GlProgram::Free(mCubeVideoSurfaceProgram);
	GlProgram::Free(m2DVideoSurfaceProgram);
	mWebcamResources.Destroy();
}

void ClientRenderer::OnVideoStreamChanged(const avs::VideoConfig &vc)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	videoConfig = vc;
	//Build Video Cubemap or perspective texture
	if (vc.use_cubemap)
	{
		scr::Texture::TextureCreateInfo textureCreateInfo =
				{
						"Cubemap Texture", videoConfig.colour_cubemap_size
						, videoConfig.colour_cubemap_size, 1, 4, 1, 1, scr::Texture::Slot::UNKNOWN
						, scr::Texture::Type::TEXTURE_CUBE_MAP, scr::Texture::Format::RGBA8
						, scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT, {}, {}
						, scr::Texture::CompressionFormat::UNCOMPRESSED
				};
		mRenderTexture->Create(textureCreateInfo);
		mRenderTexture->UseSampler(globalGraphicsResources.cubeMipMapSampler);

		mVideoSurfaceDef.graphicsCommand.Program = mCubeVideoSurfaceProgram;
	}
	else
	{
		scr::Texture::TextureCreateInfo textureCreateInfo =
				{
						"Perspective Texture", videoConfig.perspective_width
						, videoConfig.perspective_height, 1, 4, 1, 1, scr::Texture::Slot::UNKNOWN
						, scr::Texture::Type::TEXTURE_2D, scr::Texture::Format::RGBA8
						, scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT, {}, {}
						, scr::Texture::CompressionFormat::UNCOMPRESSED
				};
		mRenderTexture->Create(textureCreateInfo);
		mRenderTexture->UseSampler(globalGraphicsResources.sampler);

		mVideoSurfaceDef.graphicsCommand.Program = m2DVideoSurfaceProgram;
	}

	const float aspect = vc.perspective_width / vc.perspective_height;
	const float vertFOV = scr::GetVerticalFOVFromHorizontalInDegrees(vc.perspective_fov, aspect);
	// Takes FOV values in degrees
	ovrMatrix4f serverProj = ovrMatrix4f_CreateProjectionFov( vc.perspective_fov, vertFOV, 0.0f, 0.0f, 0.1f, 0.0f );
	videoUB.serverProj = ovrMatrix4f_Transpose(&serverProj);

	if (vc.stream_webcam)
	{
		// Set webcam uniform buffer data
		const auto ci =  mVideoTexture->GetTextureCreateInfo();
		mWebcamResources.webcamUBData.sourceTexSize = { ci.width, ci.height };
		mWebcamResources.webcamUBData.sourceOffset = { vc.webcam_offset_x, vc.webcam_offset_y };
		mWebcamResources.webcamUBData.camTexSize = { vc.webcam_width, vc.webcam_height };
	}


	//GLCheckErrorsWithTitle("Built Video Cubemap");
	//Build Lighting Cubemap
	{
		scr::Texture::TextureCreateInfo textureCreateInfo //TODO: Check this against the incoming texture from the video stream
				{
						"Cubemap Sub-Textures", 128, 128, 1, 4, 1, 3, scr::Texture::Slot::UNKNOWN
						, scr::Texture::Type::TEXTURE_CUBE_MAP, scr::Texture::Format::RGBA8
						, scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT, {}, {}
						, scr::Texture::CompressionFormat::UNCOMPRESSED
				};
		textureCreateInfo.mipCount = std::min(6,videoConfig.specular_mips);
		textureCreateInfo.width = videoConfig.specular_cubemap_size;
		textureCreateInfo.height = videoConfig.specular_cubemap_size;
		specularCubemapTexture->Create(textureCreateInfo);
		textureCreateInfo.mipCount = 1;
		textureCreateInfo.width = videoConfig.diffuse_cubemap_size;
		textureCreateInfo.height = videoConfig.diffuse_cubemap_size;
		diffuseCubemapTexture->Create(textureCreateInfo);
		textureCreateInfo.width = videoConfig.light_cubemap_size;
		textureCreateInfo.height = videoConfig.light_cubemap_size;
		mCubemapLightingTexture->Create(textureCreateInfo);
		diffuseCubemapTexture->UseSampler(globalGraphicsResources.cubeMipMapSampler);
		specularCubemapTexture->UseSampler(globalGraphicsResources.cubeMipMapSampler);
		mCubemapLightingTexture->UseSampler(globalGraphicsResources.cubeMipMapSampler);
	}
	//GLCheckErrorsWithTitle("Built Lighting Cubemap");

	// Set discard distance to the sphere detection radius of the server for use in pixel shader.
	globalGraphicsResources.scrCamera->UpdateDrawDistance(videoConfig.draw_distance);
}

void ClientRenderer::OnReceiveVideoTagData(const uint8_t *data, size_t dataSize)
{
	scr::SceneCaptureCubeTagData tagData;
	memcpy(&tagData.coreData, data, sizeof(scr::SceneCaptureCubeCoreTagData));
	avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::GlStyle,
						  tagData.coreData.cameraTransform);

	tagData.lights.resize(std::min(tagData.coreData.lightCount, (uint32_t) 4));

	scr::ServerTimestamp::setLastReceivedTimestamp(tagData.coreData.timestamp);

	// Aidan : View and proj matrices are currently unchanged from Unity
	size_t index = sizeof(scr::SceneCaptureCubeCoreTagData);
	for (auto &light : tagData.lights)
	{
		memcpy(&light, &data[index], sizeof(scr::LightTagData));
		avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::GlStyle,
							  light.worldTransform);
		index += sizeof(scr::LightTagData);
	}

	VideoTagDataCube shaderData;
	shaderData.cameraPosition = tagData.coreData.cameraTransform.position;
	shaderData.cameraRotation = tagData.coreData.cameraTransform.rotation;
	shaderData.lightCount = tagData.lights.size();

	uint32_t offset = sizeof(VideoTagDataCube) * tagData.coreData.id;
	mTagDataArrayBuffer->Update(sizeof(VideoTagDataCube), (void *) &shaderData, offset);

	videoTagDataCubeArray[tagData.coreData.id] = std::move(tagData);
}

void ClientRenderer::CopyToCubemaps(scc::GL_DeviceContext &mDeviceContext)
{
	scr::ivec2 specularOffset = {videoConfig.specular_x, videoConfig.specular_y};
	scr::ivec2 diffuseOffset = {videoConfig.diffuse_x, videoConfig.diffuse_y};
	//scr::ivec2  lightOffset={2 * specularSize+3 * specularSize / 2, specularSize * 2};
	// Here the compute shader to copy from the video texture into the cubemap/s.
	auto &tc = mRenderTexture->GetTextureCreateInfo();
	if (mRenderTexture->IsValid())
	{
		const uint32_t ThreadCount = 4;
		GLint max_u, max_v, max_w;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_u);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &max_v);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &max_w);

		scr::uvec3 size = {tc.width / ThreadCount, tc.height / ThreadCount, 6};

		size.x = std::min(size.x, (uint32_t) max_u);
		size.y = std::min(size.y, (uint32_t) max_v);
		size.z = std::min(size.z, (uint32_t) max_w);

		cubemapUB.faceSize = tc.width;
		cubemapUB.sourceOffset = {0, 0};
		cubemapUB.mip = 0;
		cubemapUB.face = 0;

		if (tc.type == scr::Texture::Type::TEXTURE_CUBE_MAP)
		{
			cubemapUB.dimensions = {cubemapUB.faceSize * 3, cubemapUB.faceSize * 2};

			scr::InputCommandCreateInfo inputCommandCreateInfo;
			inputCommandCreateInfo.effectPassName = "ColourAndDepth";

			scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, size,
												   mCopyCubemapWithDepthEffect,
												   {mColourAndDepthShaderResources});

			mDeviceContext.DispatchCompute(&inputCommand);
		}
		else
		{
			scr::uvec3 perspSize = { size.x, size.y, 1};
			const auto texInfo = mRenderTexture->GetTextureCreateInfo();
			cubemapUB.dimensions = { texInfo.width, texInfo.height };

			scr::InputCommandCreateInfo inputCommandCreateInfo;
			inputCommandCreateInfo.effectPassName = "PerspectiveColourAndDepth";

			scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, perspSize,
												   mCopyPerspectiveEffect,
												   {mCopyPerspectiveShaderResources});

			mDeviceContext.DispatchCompute(&inputCommand);
		}

		GLCheckErrorsWithTitle("Frame: CopyToCubemaps - Main");

		scr::InputCommandCreateInfo inputCommandCreateInfo;
		inputCommandCreateInfo.effectPassName = "CopyCubemap";
		scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, size,
											   mCopyCubemapEffect,
											   {mCopyCubemapShaderResources});
		cubemapUB.faceSize = 0;
		scr::ivec2 offset0 = {0, 0};
		int32_t mip_x = 0;

		if (diffuseCubemapTexture->IsValid())
		{
			static uint32_t face = 0;
			mip_x = 0;
			int32_t mip_size = videoConfig.diffuse_cubemap_size;
			uint32_t M = diffuseCubemapTexture->GetTextureCreateInfo().mipCount;
			scr::ivec2 offset = {offset0.x + diffuseOffset.x, offset0.y + diffuseOffset.y};
			for (uint32_t m = 0; m < M; m++)
			{
				inputCommand.m_WorkGroupSize = {(mip_size + ThreadCount- 1) / ThreadCount, (mip_size + ThreadCount- 1)
																						   / ThreadCount, 6};
				mCopyCubemapShaderResources.SetImageInfo(0, {diffuseCubemapTexture->GetSampler()
						, diffuseCubemapTexture, m});
				cubemapUB.sourceOffset = {offset.x+ mip_x, offset.y };
				cubemapUB.faceSize = uint32_t(mip_size);
				cubemapUB.mip = m;
				cubemapUB.face = 0;
				inputCommand.m_ShaderResources = {&mCopyCubemapShaderResources};
				mDeviceContext.DispatchCompute(&inputCommand);
				//OVR_LOG("Dispatch offset=%d %d wgSize=%d %d %d mipSize=%d",cubemapUB.sourceOffset.x,cubemapUB.sourceOffset.y,inputCommand.m_WorkGroupSize.x,inputCommand.m_WorkGroupSize.y,inputCommand.m_WorkGroupSize.z,cubemapUB.faceSize);

				mip_x += 3 * mip_size;
				mip_size /= 2;
			}
			face++;
			face = face % 6;
		}
		if (specularCubemapTexture->IsValid())
		{
			mip_x = 0;
			int32_t mip_size = videoConfig.specular_cubemap_size;
			uint32_t M = specularCubemapTexture->GetTextureCreateInfo().mipCount;
			scr::ivec2 offset = {
					offset0.x + specularOffset.x, offset0.y + specularOffset.y};
			for (uint32_t m = 0; m < M; m++)
			{
				uint32_t s=std::max(uint32_t(1),(mip_size+ThreadCount- 1) / ThreadCount);
				inputCommand.m_WorkGroupSize = {s,s, 6};
				mCopyCubemapShaderResources.SetImageInfo(
						0, {specularCubemapTexture->GetSampler(), specularCubemapTexture, m});
				cubemapUB.sourceOffset = {offset.x+ mip_x, offset.y };
				cubemapUB.faceSize = uint32_t(mip_size);
				cubemapUB.mip = m;
				cubemapUB.face = 0;
				inputCommand.m_ShaderResources = {&mCopyCubemapShaderResources};
				mDeviceContext.DispatchCompute(&inputCommand);
				mip_x += 3 * mip_size;
				mip_size /= 2;
			}
		}
		GLCheckErrorsWithTitle("Frame: CopyToCubemaps - Lighting");
		if (mVideoTexture->IsValid())
		{
			inputCommandCreateInfo.effectPassName = "ExtractTagDataID";
			scr::uvec3 size = {1, 1, 1};
			scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, size,
												   mExtractTagDataIDEffect,
												   {mExtractTagShaderResources});
			cubemapUB.faceSize = tc.width;
			cubemapUB.sourceOffset = {
					(int32_t) mVideoTexture->GetTextureCreateInfo().width - (32 * 4),
					(int32_t) mVideoTexture->GetTextureCreateInfo().height - 4};
			mDeviceContext.DispatchCompute(&inputCommand);

			inputCommandCreateInfo.effectPassName = "ExtractOneTag";
			scr::InputCommand_Compute extractTagCommand(&inputCommandCreateInfo, size,
														mExtractOneTagEffect,
														{mExtractTagShaderResources});
			mDeviceContext.DispatchCompute(&extractTagCommand);
		}
	}
	UpdateTagDataBuffers();
}

void ClientRenderer::UpdateTagDataBuffers()
{
	// TODO: too slow.
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto &cachedLights = resourceManagers->mLightManager.GetCache(cacheLock);
	VideoTagDataCube *data = static_cast<VideoTagDataCube *>(mTagDataArrayBuffer->Map());
	if (data)
	{
		//VideoTagDataCube &data=*pdata;
		for (size_t i = 0; i < videoTagDataCubeArray.size(); ++i)
		{
			const auto &td = videoTagDataCubeArray[i];
			const auto &pos = td.coreData.cameraTransform.position;
			const auto &rot = td.coreData.cameraTransform.rotation;

			data[i].cameraPosition = {pos.x, pos.y, pos.z};
			data[i].cameraRotation = {rot.x, rot.y, rot.z, rot.w};
			data[i].lightCount = td.lights.size();
			for (size_t j = 0; j < td.lights.size(); j++)
			{
				LightTag &t = data[i].lightTags[j];
				const scr::LightTagData &l = td.lights[j];
				t.uid32 = (unsigned) (((uint64_t) 0xFFFFFFFF) & l.uid);
				t.colour = *((avs::vec4 *) &l.color);
				// Convert from +-1 to [0,1]
				t.shadowTexCoordOffset.x = float(l.texturePosition[0])
										   / float(lastSetupCommand.video_config.video_width);
				t.shadowTexCoordOffset.y = float(l.texturePosition[1])
										   / float(lastSetupCommand.video_config.video_height);
				t.shadowTexCoordScale.x =
						float(l.textureSize) / float(lastSetupCommand.video_config.video_width);
				t.shadowTexCoordScale.y = float(l.textureSize)
										  / float(lastSetupCommand.video_config.video_height);
				t.position = *((avs::vec3 *) &l.position);
				ovrQuatf q = {l.orientation.x, l.orientation.y, l.orientation.z, l.orientation.w};
				// Note: we transform from a local y pointing light into the global direction.
				// this is equivalent e.g. of +z in Unity, but what about Unreal etc? Need common way
				// to express this.
				static avs::vec3 z = {0, 1.0f, 0.0f};
				t.direction = QuaternionTimesVector(q, z);
				scr::mat4 worldToShadowMatrix = scr::mat4((const float *) &l.worldToShadowMatrix);

				t.worldToShadowMatrix = *((ovrMatrix4f *) &worldToShadowMatrix);

				const auto &nodeLight = cachedLights.find(l.uid);
				if (nodeLight != cachedLights.end() && nodeLight->second.resource!=nullptr)
				{
					auto &lc		=nodeLight->second.resource->GetLightCreateInfo();
					t.is_point		=float(lc.type!=scr::Light::Type::DIRECTIONAL);
					t.is_spot		=float(lc.type==scr::Light::Type::SPOT);
					t.radius		=lc.lightRadius;
					t.range			=lc.lightRange;
					t.shadow_strength=0.0f;
				}
			}
		}
		mTagDataArrayBuffer->Unmap();
	}
}

void ClientRenderer::RenderVideo(scc::GL_DeviceContext &mDeviceContext, OVRFW::ovrRendererOutput &res)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	{
		ovrMatrix4f eye0 = res.FrameMatrices.EyeView[0];
		eye0.M[0][3] = 0.0f;
		eye0.M[1][3] = 0.0f;
		eye0.M[2][3] = 0.0f;
		ovrMatrix4f viewProj0 = res.FrameMatrices.EyeProjection[0] * eye0;
		ovrMatrix4f viewProjT0 = ovrMatrix4f_Transpose(&viewProj0);
		videoUB.invViewProj[0] = ovrMatrix4f_Inverse(&viewProjT0);
		ovrMatrix4f eye1 = res.FrameMatrices.EyeView[1];
		eye1.M[0][3] = 0.0f;
		eye1.M[1][3] = 0.0f;
		eye1.M[2][3] = 0.0f;
		ovrMatrix4f viewProj1 = res.FrameMatrices.EyeProjection[1] * eye1;
		ovrMatrix4f viewProjT1 = ovrMatrix4f_Transpose(&viewProj1);
		videoUB.invViewProj[1] = ovrMatrix4f_Inverse(&viewProjT1);
	}
	// Set data to send to the shader:
	if (mRenderTexture->IsValid())
	{
		ovrQuatf X0 = {1.0f, 0.f, 0.f, 0.0f};
		ovrQuatf headPoseQ = {clientDeviceState->headPose.orientation.x
				, clientDeviceState->headPose.orientation.y
				, clientDeviceState->headPose.orientation.z
				, clientDeviceState->headPose.orientation.w};
		ovrQuatf headPoseC = {-clientDeviceState->headPose.orientation.x
				, -clientDeviceState->headPose.orientation.y
				, -clientDeviceState->headPose.orientation.z
				, clientDeviceState->headPose.orientation.w};
		ovrQuatf xDir = QuaternionMultiply(QuaternionMultiply(headPoseQ, X0), headPoseC);
		float w = eyeSeparation / 2.0f;//.04f; //half separation.
		avs::vec4 eye = {w * xDir.x, w * xDir.y, w * xDir.z, 0.0f};
		avs::vec4 left_eye = {-eye.x, -eye.y, -eye.z, 0.0f};
		videoUB.eyeOffsets[0] = left_eye;        // left eye
		videoUB.eyeOffsets[1] = eye;    // right eye.
		videoUB.cameraPosition = clientDeviceState->headPose.position;
		videoUB.cameraRotation = clientDeviceState->headPose.orientation;
		videoUB.viewProj=res.FrameMatrices.EyeProjection[0]*res.FrameMatrices.CenterView;
		mVideoSurfaceDef.graphicsCommand.UniformData[0].Data = &(((scc::GL_Texture *) mRenderTexture.get())->GetGlTexture());
		mVideoSurfaceDef.graphicsCommand.UniformData[1].Data = &(((scc::GL_UniformBuffer *) mVideoUB.get())->GetGlBuffer());
		OVRFW::GlBuffer &buf = ((scc::GL_ShaderStorageBuffer *) globalGraphicsResources.mTagDataBuffer.get())->GetGlBuffer();
		mVideoSurfaceDef.graphicsCommand.UniformData[2].Data = &buf;
		res.Surfaces.push_back(ovrDrawSurface(&mVideoSurfaceDef));
	}
	mVideoUB->Submit();
}

void ClientRenderer::RenderLocalNodes(OVRFW::ovrRendererOutput &res)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	//Render local nodes.
	const scr::NodeManager::nodeList_t &distanceSortedRootNodes = resourceManagers->mNodeManager->GetSortedRootNodes();
	for (std::shared_ptr<scr::Node> node : distanceSortedRootNodes)
	{
		RenderNode(res, node);
		node->distance=length(globalGraphicsResources.scrCamera->GetPosition()-node->GetGlobalTransform().m_Translation);
	}

	//Render player, if parts exist.
	std::shared_ptr<scr::Node> body = resourceManagers->mNodeManager->GetBody();
	if (body)
	{
		RenderNode(res, body);
	}
	std::shared_ptr<scr::Node> leftHand = resourceManagers->mNodeManager->GetLeftHand();
	if (leftHand)
	{
		RenderNode(res, leftHand);
	}
	std::shared_ptr<scr::Node> rightHand = resourceManagers->mNodeManager->GetRightHand();
	if (rightHand)
	{
		RenderNode(res, rightHand);
	}
}

void ClientRenderer::RenderNode(OVRFW::ovrRendererOutput &res, std::shared_ptr<scr::Node> node)
{
	if(node->IsVisible())
	{
		std::shared_ptr<OVRNode> ovrNode = std::static_pointer_cast<OVRNode>(node);
		if(ovrNode->ovrSurfaceDefs.size()>0)
		{
			//Get final transform.
			scr::mat4 globalMatrix = node->GetGlobalTransform().GetTransformMatrix();
			clientDeviceState->transformToLocalOrigin = scr::mat4::Identity();//Translation(-clientDeviceState->localFootPos);
			scr::mat4 scr_Transform = clientDeviceState->transformToLocalOrigin * globalMatrix;

			//Convert transform to OVR type.
			OVR::Matrix4f transform;
			memcpy(&transform.M[0][0], &scr_Transform.a, 16 * sizeof(float));

			//Update skin uniform buffer to animate skinned meshes.
			std::shared_ptr<scr::Skin> skin = ovrNode->GetSkin();
			if(skin)
			{
				skin->UpdateBoneMatrices(globalMatrix);
			}
			GlobalGraphicsResources &globalGraphicsResources = GlobalGraphicsResources::GetInstance();
			std::vector<const scr::ShaderResource *> pbrShaderResources;
			pbrShaderResources.push_back(&globalGraphicsResources.scrCamera->GetShaderResource());
			//Push surfaces onto render queue.
			for(ovrSurfaceDef &surfaceDef : ovrNode->ovrSurfaceDefs)
			{
				// Must update the uniforms. e.g. camera pos.
				for(const scr::ShaderResource *sr : pbrShaderResources)
				{
					const std::vector<scr::ShaderResource::WriteShaderResource> &shaderResourceSet = sr->GetWriteShaderResources();
					int j = 0;
					for(const scr::ShaderResource::WriteShaderResource &resource : shaderResourceSet)
					{
						scr::ShaderResourceLayout::ShaderResourceType type = resource.shaderResourceType;
						if(type == scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER && resource.bufferInfo.buffer)
						{
							scc::GL_UniformBuffer *gl_uniformBuffer = static_cast<scc::GL_UniformBuffer *>(resource.bufferInfo.buffer);
							surfaceDef.graphicsCommand.UniformData[j].Data = &(gl_uniformBuffer->GetGlBuffer());
						}
						j++;
					}
					OVRFW::GlBuffer &buf = ((scc::GL_ShaderStorageBuffer *)globalGraphicsResources.mTagDataBuffer.get())->GetGlBuffer();
					surfaceDef.graphicsCommand.UniformData[1].Data = &buf;
				}

				res.Surfaces.emplace_back(transform, &surfaceDef);
			}
		}
	}

	//Render children.
	for(std::weak_ptr<scr::Node> childPtr : node->GetChildren())
	{
		std::shared_ptr<scr::Node> child = childPtr.lock();
		if(child)
		{
			RenderNode(res, child);
		}
	}
}

void ClientRenderer::CycleShaderMode()
{
	OVRNodeManager *nodeManager = dynamic_cast<OVRNodeManager *>(resourceManagers->mNodeManager.get());
	passSelector++;
	passSelector = passSelector % (passNames.size());
	nodeManager->ChangeEffectPass(passNames[passSelector].c_str());
}

void ClientRenderer::ListNode(const std::shared_ptr<scr::Node>& node, int indent, size_t& linesRemaining)
{
	//Return if we do not want to print any more lines.
	if(linesRemaining <= 0)
	{
		return;
	}
	--linesRemaining;

	//Set indent string to indent amount.
	static char indent_txt[20];
	indent_txt[indent] = 0;
	if(indent > 0)
	{
		indent_txt[indent - 1] = ' ';
	}

	//Retrieve info string on mesh on node, if the node has a mesh.
	std::string meshInfoString;
	const std::shared_ptr<scr::Mesh>& mesh = node->GetMesh();
	if(mesh)
	{
		meshInfoString = "mesh ";
		meshInfoString+=mesh->GetMeshCreateInfo().name.c_str();
	}
	avs::vec3 pos=node->GetGlobalPosition();
	//Print details on node to screen.
	OVR_LOG("%s%llu %s (%4.4f,%4.4f,%4.4f) %s", indent_txt, node->id, node->name.c_str()
			,pos.x,pos.y,pos.z
			, meshInfoString.c_str());

	//Print information on children to screen.
	const std::vector<std::weak_ptr<scr::Node>>& children = node->GetChildren();
	for(const auto c : children)
	{
		ListNode(c.lock(), indent + 1, linesRemaining);
	}
}

void ClientRenderer::WriteDebugOutput()
{
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto& rootNodes = resourceManagers->mNodeManager->GetRootNodes();
	OVR_LOG("Root Nodes: %zu   Total Nodes: %zu",rootNodes.size(),resourceManagers->mNodeManager->GetNodeAmount());
	size_t linesRemaining = 2000;
	static uid show_only=0;
	for(const std::shared_ptr<scr::Node>& node : rootNodes)
	{
		if(show_only!=0&&show_only!=node->id)
			continue;
		ListNode(node, 1, linesRemaining);
		if(linesRemaining <= 0)
		{
			break;
		}
	}
}

void ClientRenderer::CycleOSD()
{
	show_osd = (show_osd + 1) % NUM_OSDS;
}

void ClientRenderer::SetStickOffset(float x, float y)
{
	//ovrFrameInput *f = const_cast<ovrFrameInput *>(&vrFrame);
	//f->Input.sticks[0][0] += dx;
	//f->Input.sticks[0][1] += dy;
}

void ClientRenderer::DrawOSD()
{
	static avs::vec3 offset={0,0,4.5f};
	static avs::vec4 colour={1.0f,0.7f,0.5f,0.5f};
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	if(passSelector!=0)
	{
		static avs::vec3 passoffset={0,2.0f,5.0f};
		clientAppInterface->PrintText(passoffset,colour,"%s",globalGraphicsResources.effectPassName);
	}
	auto ctr = mNetworkSource.getCounterValues();

	switch(show_osd)
	{
		case NO_OSD:
			break;
		case CAMERA_OSD:
		{
			avs::vec3 vidPos(0, 0, 0);
			if(videoTagDataCubeArray.size())
			{
				vidPos = videoTagDataCubeArray[0].coreData.cameraTransform.position;
			}
			clientAppInterface->PrintText(
					offset, colour,
					"        Foot pos: %1.2f, %1.2f, %1.2f    yaw: %1.2f\n"
					" Camera Relative: %1.2f, %1.2f, %1.2f Abs: %1.2f, %1.2f, %1.2f\n"
					"  Video Position: %1.2f, %1.2f, %1.2f\n"
					"Controller 0 rel: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
					"             abs: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
					"Controller 1 rel: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
					"             abs: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
					, clientDeviceState->originPose.position.x,	clientDeviceState->originPose.position.y,clientDeviceState->originPose.position.z,
					clientDeviceState->relativeHeadPos.x, clientDeviceState->relativeHeadPos.y,clientDeviceState->relativeHeadPos.z,
					clientDeviceState->headPose.position.x, clientDeviceState->headPose.position.y,clientDeviceState->headPose.position.z,
					clientDeviceState->stickYaw,
					vidPos.x, vidPos.y, vidPos.z,
					clientDeviceState->controllerRelativePoses[0].position.x,
					clientDeviceState->controllerRelativePoses[0].position.y,
					clientDeviceState->controllerRelativePoses[0].position.z,
					clientDeviceState->controllerRelativePoses[0].orientation.x,
					clientDeviceState->controllerRelativePoses[0].orientation.y,
					clientDeviceState->controllerRelativePoses[0].orientation.z,
					clientDeviceState->controllerPoses[0].position.x,
					clientDeviceState->controllerPoses[0].position.y,
					clientDeviceState->controllerPoses[0].position.z,
					clientDeviceState->controllerPoses[0].orientation.x,
					clientDeviceState->controllerPoses[0].orientation.y,
					clientDeviceState->controllerPoses[0].orientation.z,
					clientDeviceState->controllerRelativePoses[1].position.x,
					clientDeviceState->controllerRelativePoses[1].position.y,
					clientDeviceState->controllerRelativePoses[1].position.z,
					clientDeviceState->controllerRelativePoses[1].orientation.x,
					clientDeviceState->controllerRelativePoses[1].orientation.y,
					clientDeviceState->controllerRelativePoses[1].orientation.z,
					clientDeviceState->controllerPoses[1].position.x,
					clientDeviceState->controllerPoses[1].position.y,
					clientDeviceState->controllerPoses[1].position.z,
					clientDeviceState->controllerPoses[1].orientation.x,
					clientDeviceState->controllerPoses[1].orientation.y,
					clientDeviceState->controllerPoses[1].orientation.z
			);

			break;
		}
		case NETWORK_OSD:
		{
			clientAppInterface->PrintText(
					offset, colour,
					"Frames: %d\nPackets Dropped: Network %d | Decoder %d\n"
					"Incomplete Decoder Packets: %d\n"
					"Bandwidth(kbps): %4.4f"
					, mDecoder.getTotalFramesProcessed(),
					ctr.networkPacketsDropped,
					ctr.decoderPacketsDropped,
					ctr.incompleteDecoderPacketsReceived,
					ctr.bandwidthKPS);
			break;
		}
		case GEOMETRY_OSD:
		{
			std::ostringstream str;
			const scr::NodeManager::nodeList_t &rootNodes = resourceManagers->mNodeManager->GetRootNodes();

			str <<"Nodes: "<<static_cast<uint64_t>(resourceManagers->mNodeManager->GetNodeAmount())<<
				"Orphans: "<<ctr.m_packetMapOrphans<<"\n";
			for(std::shared_ptr<scr::Node> node : rootNodes)
			{
				str << node->id << ", ";
			}
			str << "\n";

			const auto &missingResources = resourceCreator->GetMissingResources();
			if(missingResources.size() > 0)
			{
				str << "Missing Resources\n";

				size_t resourcesOnLine = 0;
				for(const auto &missingPair : missingResources)
				{
					const ResourceCreator::MissingResource &missingResource = missingPair.second;
					str << missingResource.resourceType << "_" << missingResource.id;

					resourcesOnLine++;
					if(resourcesOnLine >= MAX_RESOURCES_PER_LINE)
					{
						str << std::endl;
						resourcesOnLine = 0;
					}
					else
					{
						str << " | ";
					}
				}
			}

			clientAppInterface->PrintText(
					offset, colour, str.str().c_str()
			);

			break;
		}
		case TAG_OSD:
		{
			std::ostringstream sstr;
			std::setprecision(5);
			sstr << "Tags\n" << std::setw(4);
			static int ii = 0;
			static char iii = 0;
			iii++;
			if(!iii)
			{
				ii++;
				if(ii > 2)
				{
					ii = 0;
				}
			}
			for(size_t i = 0; i < std::min((size_t)8, videoTagDataCubeArray.size()); i++)
			{
				auto &tag = videoTagDataCubeArray[i + 8 * ii];
				sstr << tag.coreData.lightCount << " lights\n";
				for(size_t j = 0; j < tag.lights.size(); j++)
				{
					auto &l = tag.lights[j];
					sstr << "\t" << l.uid << ": Type " << ToString((scr::Light::Type)l.lightType)
						 << ", clr " << l.color.x << "," << l.color.y << "," << l.color.z;
					if(l.lightType == scr::LightType::Directional)
					{
						ovrQuatf q = {l.orientation.x, l.orientation.y, l.orientation.z, l.orientation.w};
						avs::vec3 z = {0, 0, 1.0f};
						avs::vec3 direction = QuaternionTimesVector(q, z);
						sstr << ", d " << direction.x << "," << direction.y << "," << direction.z;
					}
					sstr << "\n";
				}
			}
			clientAppInterface->PrintText(offset, colour, sstr.str().c_str());

			break;
		}
		case CONTROLLER_OSD:
		{
			avs::vec3 leftHandPosition, rightHandPosition;
			avs::vec4 leftHandOrientation, rightHandOrientation;

			std::shared_ptr<scr::Node> leftHand = resourceManagers->mNodeManager->GetLeftHand();
			if(leftHand)
			{
				leftHandPosition = leftHand->GetGlobalTransform().m_Translation;
				leftHandOrientation = leftHand->GetGlobalTransform().m_Rotation;
			}
			std::shared_ptr<scr::Node> rightHand = resourceManagers->mNodeManager->GetRightHand();
			if(rightHand)
			{
				rightHandPosition = rightHand->GetGlobalTransform().m_Translation;
				rightHandOrientation = rightHand->GetGlobalTransform().m_Rotation;
			}

			clientAppInterface->PrintText(
							offset, colour,
							"Controller 0 rel: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
							"             abs: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
							"Controller 1 rel: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
							"             abs: (%1.2f, %1.2f, %1.2f) {%1.2f, %1.2f, %1.2f}\n"
							,clientDeviceState->controllerRelativePoses[0].position.x,
							clientDeviceState->controllerRelativePoses[0].position.y,
							clientDeviceState->controllerRelativePoses[0].position.z,
							clientDeviceState->controllerRelativePoses[0].orientation.x,
							clientDeviceState->controllerRelativePoses[0].orientation.y,
							clientDeviceState->controllerRelativePoses[0].orientation.z,
							clientDeviceState->controllerPoses[0].position.x,
							clientDeviceState->controllerPoses[0].position.y,
							clientDeviceState->controllerPoses[0].position.z,
							clientDeviceState->controllerPoses[0].orientation.x,
							clientDeviceState->controllerPoses[0].orientation.y,
							clientDeviceState->controllerPoses[0].orientation.z,
							clientDeviceState->controllerRelativePoses[1].position.x,
							clientDeviceState->controllerRelativePoses[1].position.y,
							clientDeviceState->controllerRelativePoses[1].position.z,
							clientDeviceState->controllerRelativePoses[1].orientation.x,
							clientDeviceState->controllerRelativePoses[1].orientation.y,
							clientDeviceState->controllerRelativePoses[1].orientation.z,
							clientDeviceState->controllerPoses[1].position.x,
							clientDeviceState->controllerPoses[1].position.y,
							clientDeviceState->controllerPoses[1].position.z,
							clientDeviceState->controllerPoses[1].orientation.x,
							clientDeviceState->controllerPoses[1].orientation.y,
							clientDeviceState->controllerPoses[1].orientation.z
					);

			break;
		}
		default:
			break;
	}
}