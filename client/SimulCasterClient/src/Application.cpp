// (C) Copyright 2018-2019 Simul Software Ltd
#include "Application.h"

#include <sstream>

#include "GLESDebug.h"
#include "GuiSys.h"
#include "OVR_FileSys.h"
#include "OVR_GlUtils.h"
#include "OVR_Locale.h"
#include "OVR_LogUtils.h"
#include "OVR_Math.h"

#include "enet/enet.h"

#include "AndroidDiscoveryService.h"
#include "Config.h"
#include "OVRActorManager.h"
#include "VideoSurface.h"

#if defined( OVR_OS_WIN32 )
#include "../res_pc/resource.h"
#endif

using namespace OVR;


#if defined( OVR_OS_ANDROID )
extern "C" {

jlong Java_co_Simul_remoteplayclient_MainActivity_nativeSetAppInterface(JNIEnv* jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	VideoDecoderProxy::InitializeJNI(jni);
	return (new Application())->SetActivity(jni, clazz, activity, fromPackageName, commandString, uriString);
}

} // extern "C"

#endif

const int specularSize = 128;
const int diffuseSize = 64;
const int lightSize = 64;
scr::ivec2 specularOffset={0,0};
scr::ivec2 diffuseOffset={3* specularSize/2, specularSize*2};
scr::ivec2 roughOffset={3* specularSize,0};
scr::ivec2  lightOffset={2 * specularSize+3 * specularSize / 2, specularSize * 2};

ovrQuatf QuaternionMultiply(const ovrQuatf &p,const ovrQuatf &q)
{
	ovrQuatf r;
	r.w= p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
	r.x= p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
	r.y= p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z;
	r.z= p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x;
	return r;
}

static inline ovrQuatf RelativeQuaternion(const ovrQuatf &p,const ovrQuatf &q)
{
	ovrQuatf iq=q;
	iq.x*=-1.f;
	iq.y*=-1.f;
	iq.z*=-1.f;
	return QuaternionMultiply(p,iq);
}

Application::Application()
	: mDecoder(avs::DecoderBackend::Custom)
	, mPipelineConfigured(false)
	, resourceCreator(basist::transcoder_texture_format::cTFETC2)
	, mSoundEffectContext(nullptr)
	, mSoundEffectPlayer(nullptr)
	, mGuiSys(OvrGuiSys::Create())
	, mLocale(nullptr)
	, mVideoSurfaceTexture(nullptr)
	, mCubemapTexture(nullptr)
	, mCubemapLightingTexture(nullptr)
	, mCameraPositionBuffer(nullptr)
	, mOvrMobile(nullptr)
	, mSession(this, std::make_unique<AndroidDiscoveryService>(), resourceCreator)
	, mControllerID(0)
	, resourceManagers(new OVRActorManager)
	, mDeviceContext(dynamic_cast<scr::RenderPlatform*>(&GlobalGraphicsResources.renderPlatform))
{
	pthread_setname_np(pthread_self(), "SimulCaster_Application");
	memset(&renderConstants,0,sizeof(RenderConstants));
	renderConstants.colourOffsetScale={0.0f,0.0f,1.0f,0.6667f};
	renderConstants.depthOffsetScale={0.0f,0.6667f,0.5f,0.3333f};
	mContext.setMessageHandler(Application::avsMessageHandler, this);

	if(enet_initialize() != 0) {
		OVR_FAIL("Failed to initialize ENET library");
	}

	resourceCreator.Initialise(dynamic_cast<scr::RenderPlatform*>(&GlobalGraphicsResources.renderPlatform), scr::VertexBufferLayout::PackingStyle::INTERLEAVED);
	resourceCreator.AssociateResourceManagers(&resourceManagers.mIndexBufferManager, &resourceManagers.mShaderManager, &resourceManagers.mMaterialManager, &resourceManagers.mTextureManager, &resourceManagers.mUniformBufferManager, &resourceManagers.mVertexBufferManager, &resourceManagers.mMeshManager, &resourceManagers.mLightManager);
	resourceCreator.AssociateActorManager(resourceManagers.mActorManager.get());

	//Default Effects
	scr::Effect::EffectCreateInfo ci;
	ci.effectName = "StandardEffects";
	GlobalGraphicsResources.pbrEffect.Create(&ci);

	//Default Sampler
	scr::Sampler::SamplerCreateInfo sci  = {};
	sci.wrapU = scr::Sampler::Wrap::REPEAT;
	sci.wrapV = scr::Sampler::Wrap::REPEAT;
	sci.wrapW = scr::Sampler::Wrap::REPEAT;
	sci.minFilter = scr::Sampler::Filter::LINEAR;
	sci.magFilter = scr::Sampler::Filter::LINEAR;

	GlobalGraphicsResources.sampler = GlobalGraphicsResources.renderPlatform.InstantiateSampler();
	GlobalGraphicsResources.sampler->Create(&sci);

	sci.minFilter = scr::Sampler::Filter::MIPMAP_LINEAR;
	GlobalGraphicsResources.cubeMipMapSampler = GlobalGraphicsResources.renderPlatform.InstantiateSampler();
	GlobalGraphicsResources.cubeMipMapSampler->Create(&sci);
}

Application::~Application()
{
	mPipeline.deconfigure();

	mOvrMobile=nullptr;
	mRefreshRates.clear();
	delete mVideoSurfaceTexture;
	mVideoSurfaceDef.geo.Free();
	GlProgram::Free(mVideoSurfaceProgram);

	delete mSoundEffectPlayer;
	delete mSoundEffectContext;

	OvrGuiSys::Destroy(mGuiSys);

	mSession.Disconnect(REMOTEPLAY_TIMEOUT);
	enet_deinitialize();
}

void Application::Configure(ovrSettings& settings )
{
	settings.CpuLevel = 0;
	settings.GpuLevel = 2;

	settings.EyeBufferParms.colorFormat = COLOR_8888;
	settings.EyeBufferParms.depthFormat = DEPTH_16;
	settings.EyeBufferParms.multisamples = 1;
	settings.TrackingSpace=VRAPI_TRACKING_SPACE_LOCAL;
	//settings.TrackingTransform = VRAPI_TRACKING_TRANSFORM_SYSTEM_CENTER_EYE_LEVEL;
	settings.RenderMode = RENDERMODE_STEREO;
}

void Application::EnteredVrMode(const ovrIntentType intentType, const char* intentFromPackage, const char* intentJSON, const char* intentURI )
{

	if(intentType == INTENT_LAUNCH)
	{
		const ovrJava *java = app->GetJava();

		OVR_LOG("%s | %s", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &GlobalGraphicsResources.maxFragTextureSlots);
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &GlobalGraphicsResources.maxFragUniformBlocks);
		OVR_LOG("Fragment Texture Slots: %d, Fragment Uniform Blocks: %d", GlobalGraphicsResources.maxFragTextureSlots, GlobalGraphicsResources.maxFragUniformBlocks);

        //Setup Debug
        scc::SetupGLESDebug();

		mOvrMobile = app->GetOvrMobile();

		mSoundEffectContext = new ovrSoundEffectContext(*java->Env, java->ActivityObject);
		mSoundEffectContext->Initialize(&app->GetFileSys());
		mSoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

		mLocale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");

		std::string fontName;
		GetLocale().GetString("@string/font_name", "efigs.fnt", fontName);
		mGuiSys->Init(this->app, *mSoundEffectPlayer, fontName.c_str(), &app->GetDebugLines());

		//VideoSurfaceProgram
		{
			{
				mVideoUB = GlobalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
				scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo = {2, sizeof(VideoUB), &videoUB};
				mVideoUB->Create(&uniformBufferCreateInfo);
			}
			static ovrProgramParm uniformParms[] =    // both TextureMvpProgram and CubeMapPanoProgram use the same parm mapping
										  {
												  {"colourOffsetScale"	, ovrProgramParmType::FLOAT_VECTOR4},
												  {"depthOffsetScale"	, ovrProgramParmType::FLOAT_VECTOR4},
												  {"cubemapTexture"		, ovrProgramParmType::TEXTURE_SAMPLED},
												  {"videoFrameTexture"	, ovrProgramParmType::TEXTURE_SAMPLED},
												  {"videoUB"			, ovrProgramParmType::BUFFER_UNIFORM},
										  };
			std::string videoSurfaceVert = LoadTextFile("shaders/VideoSurface.vert");
			std::string videoSurfaceFrag = LoadTextFile("shaders/VideoSurface.frag");
			mVideoSurfaceProgram = GlProgram::Build(
					nullptr, videoSurfaceVert.c_str(),
					"#extension GL_OES_EGL_image_external_essl3 : require\n", videoSurfaceFrag.c_str(),
					uniformParms, sizeof(uniformParms) / sizeof(ovrProgramParm),
					310);
			if (!mVideoSurfaceProgram.IsValid())
			{
				OVR_FAIL("Failed to build video surface shader program");
			}
		}
		mDecoder.setBackend(new VideoDecoderProxy(java->Env, this, avs::VideoCodec::HEVC));

		mVideoSurfaceTexture = new OVR::SurfaceTexture(java->Env);
		mVideoTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mCubemapUB = GlobalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
		mCubemapTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();
        mDiffuseTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mSpecularTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mRoughSpecularTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();

		mCubemapLightingTexture = GlobalGraphicsResources.renderPlatform.InstantiateTexture();
		mCameraPositionBuffer= GlobalGraphicsResources.renderPlatform.InstantiateShaderStorageBuffer();
		{
			scr::ShaderStorageBuffer::ShaderStorageBufferCreateInfo shaderStorageBufferCreateInfo = {
					3,
					scr::ShaderStorageBuffer::Access::NONE,
					8*4*sizeof(float),
					(void*)&mCameraPositions
			};
			mCameraPositionBuffer->Create(&shaderStorageBufferCreateInfo);
		}
		{
			CopyCubemapSrc     = LoadTextFile("shaders/CopyCubemap.comp");
			mCopyCubemapEffect = GlobalGraphicsResources.renderPlatform.InstantiateEffect();
			mCopyCubemapWithDepthEffect = GlobalGraphicsResources.renderPlatform.InstantiateEffect();
			mExtractCameraPositionEffect=GlobalGraphicsResources.renderPlatform.InstantiateEffect();

			scr::Effect::EffectCreateInfo effectCreateInfo = {};
			effectCreateInfo.effectName = "CopyCubemap";
			mCopyCubemapEffect->Create(&effectCreateInfo);

			effectCreateInfo.effectName = "CopyCubemapWithDepth";
			mCopyCubemapWithDepthEffect->Create(&effectCreateInfo);

			effectCreateInfo.effectName = "ExtractCameraPosition";
			mExtractCameraPositionEffect->Create(&effectCreateInfo);

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
				ExtractPositionSrc     = LoadTextFile("shaders/ExtractPosition.comp");
				pipelineCreateInfo.m_ShaderCreateInfo[0].filepath   = "shaders/ExtractPosition.comp";
				pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = ExtractPositionSrc;
				pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "extract_camera_position";
				pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "extract_camera_position";
				scr::ShaderSystem::Pipeline cp3(&GlobalGraphicsResources.renderPlatform, &pipelineCreateInfo);
				effectPassCreateInfo.effectPassName = "ExtractPosition";
				effectPassCreateInfo.pipeline = cp3;
				mExtractCameraPositionEffect->CreatePass(&effectPassCreateInfo);


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
			sr.AddBuffer(2, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER, 3, "CameraPosition", {mCameraPositionBuffer.get()});


			mCubemapComputeShaderResources.push_back(sr);

			mCopyCubemapEffect->LinkShaders("CopyCubemap", {});
			mCopyCubemapWithDepthEffect->LinkShaders("ColourAndDepth",{});
			mExtractCameraPositionEffect->LinkShaders("ExtractPosition",{});
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

		//Set scr::EffectPass
		scr::ShaderSystem::PipelineCreateInfo pipelinePBR;
		{
			pipelinePBR.m_Count                          = 2;
			pipelinePBR.m_PipelineType                   = scr::ShaderSystem::PipelineType::PIPELINE_TYPE_GRAPHICS;
			pipelinePBR.m_ShaderCreateInfo[0].stage      = scr::Shader::Stage::SHADER_STAGE_VERTEX;
			pipelinePBR.m_ShaderCreateInfo[0].entryPoint = "main";
			pipelinePBR.m_ShaderCreateInfo[0].filepath   = "shaders/OpaquePBR.vert";
			pipelinePBR.m_ShaderCreateInfo[0].sourceCode = LoadTextFile("shaders/OpaquePBR.vert");
			pipelinePBR.m_ShaderCreateInfo[1].stage      = scr::Shader::Stage::SHADER_STAGE_FRAGMENT;
			pipelinePBR.m_ShaderCreateInfo[1].entryPoint = "Opaque";
			pipelinePBR.m_ShaderCreateInfo[1].filepath   = "shaders/OpaquePBR.frag";
			pipelinePBR.m_ShaderCreateInfo[1].sourceCode = LoadTextFile("shaders/OpaquePBR.frag");
		}

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
		scr::ShaderResourceLayout fragLayout;
		fragLayout.AddBinding(3, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		fragLayout.AddBinding(10, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		fragLayout.AddBinding(11, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		fragLayout.AddBinding(12, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		fragLayout.AddBinding(13, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		fragLayout.AddBinding(14, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		fragLayout.AddBinding(15, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		fragLayout.AddBinding(16, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);

		scr::ShaderResource pbrShaderResource({vertLayout, fragLayout});
		pbrShaderResource.AddBuffer(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 0, "u_CameraData", {});
		pbrShaderResource.AddBuffer(1, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 3, "u_MaterialData", {});
		pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 10, "u_Diffuse", {});
		pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 11, "u_Normal", {});
		pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 12, "u_Combined", {});
		pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 13, "u_DiffuseCubemap", {});
		pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 14, "u_SpecularCubemap", {});
		pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 15, "u_RoughSpecularCubemap", {});

		BuildEffectPass("OpaquePBR", &layout, &pipelinePBR, {pbrShaderResource});

		//Set Lighting Cubemap Shader Resource
        scr::ShaderResourceLayout lightingCubemapLayout;
        lightingCubemapLayout.AddBinding(13, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
        lightingCubemapLayout.AddBinding(14, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		lightingCubemapLayout.AddBinding(15, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		lightingCubemapLayout.AddBinding(16, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);

        GlobalGraphicsResources.lightCubemapShaderResources.SetLayouts({lightingCubemapLayout});
		GlobalGraphicsResources.lightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 13, "u_DiffuseCubemap", {mDiffuseTexture->GetSampler(), mDiffuseTexture});
		GlobalGraphicsResources.lightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 14, "u_SpecularCubemap", {mSpecularTexture->GetSampler(), mSpecularTexture});
		GlobalGraphicsResources.lightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 15, "u_RoughSpecularCubemap", {mRoughSpecularTexture->GetSampler(), mRoughSpecularTexture});
		GlobalGraphicsResources.lightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 16, "u_LightsCubemap", {mCubemapLightingTexture->GetSampler(), mCubemapLightingTexture});

		int num_refresh_rates=vrapi_GetSystemPropertyInt(java,VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
		mRefreshRates.resize(num_refresh_rates);
		vrapi_GetSystemPropertyFloatArray(java,VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,mRefreshRates.data(),num_refresh_rates);

		if(num_refresh_rates>0)
			vrapi_SetDisplayRefreshRate(mOvrMobile,mRefreshRates[num_refresh_rates-1]);
	}
}

void Application::LeavingVrMode()
{
}

bool Application::OnKeyEvent(const int keyCode, const int repeatCount, const KeyEventType eventType)
{
    return mGuiSys->OnKeyEvent(keyCode, repeatCount, eventType);
}

extern ovrQuatf QuaternionMultiply(const ovrQuatf &p,const ovrQuatf &q);

ovrFrameResult Application::Frame(const ovrFrameInput& vrFrame)
{
    GL_CheckErrors("Frame: Start");
	// process input events first because this mirrors the behavior when OnKeyEvent was
	// a virtual function on VrAppInterface and was called by VrAppFramework.
	for(int i = 0; i < vrFrame.Input.NumKeyEvents; i++)
	{
		const int keyCode = vrFrame.Input.KeyEvents[i].KeyCode;
		const int repeatCount = vrFrame.Input.KeyEvents[i].RepeatCount;
		const KeyEventType eventType = vrFrame.Input.KeyEvents[i].EventType;

		if(OnKeyEvent(keyCode, repeatCount, eventType))
		{
			continue;   // consumed the event
		}
		// If nothing consumed the key and it's a short-press of the back key, then exit the application to OculusHome.
		if(keyCode == OVR_KEY_BACK && eventType == KEY_EVENT_SHORT_PRESS)
		{
			app->ShowConfirmQuitSystemUI();
			continue;
		}
	}

	// Try to find remote controller
	if((int)mControllerID == 0) {
		InitializeController();
	}

	// Query controller input state.
	ControllerState controllerState = {};
	if((int)mControllerID != 0)
	{
		ovrInputStateTrackedRemote ovrState;
		ovrState.Header.ControllerType = ovrControllerType_TrackedRemote;
		if(vrapi_GetCurrentInputState(mOvrMobile, mControllerID, &ovrState.Header) >= 0)
		{
			controllerState.mButtons = ovrState.Buttons;

			//Flip show debug information, if the grip trigger was released.
			if((mLastPrimaryControllerState.mButtons & ovrButton::ovrButton_GripTrigger) != 0 && (controllerState.mButtons & ovrButton::ovrButton_GripTrigger) == 0)
			{
					mShowInfo=!mShowInfo;
            }

			controllerState.mTrackpadStatus = ovrState.TrackpadStatus > 0;
			controllerState.mTrackpadX = ovrState.TrackpadPosition.x / mTrackpadDim.x;
			controllerState.mTrackpadY = ovrState.TrackpadPosition.y / mTrackpadDim.y;
			controllerState.mJoystickAxisX=ovrState.Joystick.x;
			controllerState.mJoystickAxisY=ovrState.Joystick.y * -1;
			if(controllerState.mTrackpadStatus)
			{
				float          dx = controllerState.mTrackpadX - 0.5f;
				float          dy = controllerState.mTrackpadY - 0.5f;
				ModelCollision collisionModel;
				ModelCollision groundCollisionModel;

				//					 collisionModel, groundCollisionModel );
				ovrFrameInput *f = const_cast<ovrFrameInput *>(&vrFrame);
				f->Input.sticks[0][0] += dx;
				f->Input.sticks[0][1] += dy;
				//f->Input.sticks[1][0] += dx;
				//f->Input.sticks[1][1] += dy;
			}
//ovr_re
		}
	}
	ovrVector3f footPos=mScene.GetFootPos();

	//Get HMD Position/Orientation
	scr::vec3 headPos =*((const scr::vec3*)&vrFrame.Tracking.HeadPose.Pose.Position);
	headPos+=*((scr::vec3*)&footPos);
	//headPos*=10.0f;
	scr::vec3 scr_OVR_headPos = {headPos.x, headPos.y, headPos.z};

	//Get the Capture Position
	scr::Transform::TransformCreateInfo tci = {(scr::RenderPlatform*)(&GlobalGraphicsResources.renderPlatform)};
	scr::Transform scr_UE4_captureTransform(tci);
	avs::Transform avs_UE4_captureTransform = mDecoder.getCameraTransform();
	scr_UE4_captureTransform = avs_UE4_captureTransform;

	if(mDecoder.hasValidTransform())
	{
		if (!receivedInitialPos)
		{
			// Oculus Origin means where the headset's zero is in real space.
			oculusOrigin = scr_UE4_captureTransform.m_Translation;
			receivedInitialPos = true;
		}
	}
	if(!receivedInitialPos)
	{
		scr_UE4_captureTransform = avs::Transform();
		oculusOrigin = scr_UE4_captureTransform.m_Translation;
	}

	cameraPosition = oculusOrigin+scr_OVR_headPos;

	// Handle networked session.
	if(mSession.IsConnected())
	{
		avs::DisplayInfo displayInfo = {1440, 1600};
		headPose.orientation=*((avs::vec4*)(&vrFrame.Tracking.HeadPose.Pose.Orientation));
		headPose.position = {cameraPosition.x, cameraPosition.y, cameraPosition.z};

		mSession.Frame(displayInfo, headPose, controllerPoses, receivedInitialPos, controllerState, mDecoder.idrRequired());
		if (!receivedInitialPos&&mSession.receivedInitialPos)
		{
			oculusOrigin = mSession.GetInitialPos();
			receivedInitialPos = true;
		}
	}
	else
	{
		ENetAddress remoteEndpoint;
		if(mSession.Discover(REMOTEPLAY_DISCOVERY_PORT, remoteEndpoint))
		{
			mSession.Connect(remoteEndpoint, REMOTEPLAY_TIMEOUT);
		}
	}
	mLastPrimaryControllerState = controllerState;

	// Update video texture if we have any pending decoded frames.
	while(mNumPendingFrames > 0)
	{
		mVideoSurfaceTexture->Update();
		--mNumPendingFrames;
	}

	// Process stream pipeline
	mPipeline.process();

	//Build frame
	ovrFrameResult res;

	mScene.Frame(vrFrame);
	mScene.GetFrameMatrices(vrFrame.FovX, vrFrame.FovY, res.FrameMatrices);
	mScene.GenerateFrameSurfaceList(res.FrameMatrices, res.Surfaces);

	// Update GUI systems after the app frame, but before rendering anything.
	mGuiSys->Frame(vrFrame, res.FrameMatrices.CenterView);
	scr::vec3 camera_from_videoCentre=cameraPosition-scr_UE4_captureTransform.m_Translation;
	// The camera should be where our head is. But when rendering, the camera is in OVR space, so:
	GlobalGraphicsResources.scrCamera->UpdatePosition(scr_OVR_headPos);

	static float frameRate=1.0f;
	if(vrFrame.DeltaSeconds>0.0f)
	{
		frameRate*=0.99f;
		frameRate+=0.01f/vrFrame.DeltaSeconds;
	}

	Quat<float> headPose = vrFrame.Tracking.HeadPose.Pose.Orientation;
	ovrQuatf X0={1.0f,0.f,0.f,0.0f};
	ovrQuatf headPoseC={-headPose.x,-headPose.y,-headPose.z,headPose.w};
	ovrQuatf xDir= QuaternionMultiply(QuaternionMultiply(headPose,X0),headPoseC);
#if 1
    std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto ctr=mNetworkSource.getCounterValues();
	if(mSession.IsConnected())
	{
		if(mShowInfo)
			mGuiSys->ShowInfoText(
				0.017f,
				"Frames: %d\nPackets Dropped: Network %d | Decoder %d\n"
				"Incomplete Decoder Packets: %d\n"
				"Framerate: %4.4f Bandwidth(kbps): %4.4f\n"
				"Actors: %d \n"
				"Camera Position: %1.3f, %1.3f, %1.3f\n"
				//"Orient: %1.3f, {%1.3f, %1.3f, %1.3f}\n"
				//"Pos: %3.3f %3.3f %3.3f\n"
				"Orphans: %d\n",
				mDecoder.getTotalFramesProcessed(), ctr.networkPacketsDropped, ctr.decoderPacketsDropped,
				ctr.incompleteDPsReceived,
				frameRate, ctr.bandwidthKPS,
				static_cast<uint64_t>(resourceManagers.mActorManager->GetActorList().size()),
				cameraPosition.x, cameraPosition.y, cameraPosition.z,
				//headPose.w, headPose.x, headPose.y, headPose.z,
				//headPos.x, headPos.y, headPos.z,
				ctr.m_packetMapOrphans);
	}
	else
	{
		mGuiSys->ShowInfoText(0.001f,"Disconnected");
	};
#endif
	res.FrameIndex   = vrFrame.FrameNumber;
	res.DisplayTime  = vrFrame.PredictedDisplayTimeInSeconds;
	res.SwapInterval = app->GetSwapInterval();

	res.FrameFlags = 0;
	res.LayerCount = 0;

	ovrLayerProjection2& worldLayer = res.Layers[res.LayerCount++].Projection;

	worldLayer = vrapi_DefaultLayerProjection2();
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
	worldLayer.HeadPose = vrFrame.Tracking.HeadPose;
	for(int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		worldLayer.Textures[eye].ColorSwapChain = vrFrame.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = vrFrame.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
	}

	GL_CheckErrors("Frame: Pre-Cubemap");
	CopyToCubemaps();
	// Append video surface
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
	mVideoSurfaceDef.graphicsCommand.UniformData[4].Data =  &(((scc::GL_UniformBuffer *)  mVideoUB.get())->GetGlBuffer());
	if(mCubemapTexture->IsValid())
	{
		float w=vrFrame.IPD/2.0f;//.04f; //half separation.
		scr::vec4 eye={w*xDir.x,w*xDir.y,w*xDir.z,0.0f};
		scr::vec3 &v=camera_from_videoCentre;
		scr::vec4 right_eye ={v.x+eye.x,v.y+eye.y,v.z+eye.z,0.0f};
		scr::vec4 left_eye ={-eye.x,-eye.y,-eye.z,0.0f};
		videoUB.eyeOffsets[0]=left_eye;		// left eye
		videoUB.eyeOffsets[1]=eye;	// right eye.
		videoUB.cameraPosition=cameraPosition;

		mVideoSurfaceDef.graphicsCommand.UniformData[2].Data = &(((scc::GL_Texture *) mCubemapTexture.get())->GetGlTexture());
		mVideoSurfaceDef.graphicsCommand.UniformData[3].Data = &(((scc::GL_Texture *)  mVideoTexture.get())->GetGlTexture());
		res.Surfaces.push_back(ovrDrawSurface(&mVideoSurfaceDef));
	}
	mVideoUB->Submit();

	//Move the hands before they are drawn.
	UpdateHandObjects();
	//Append SCR Actors to surfaces.
	GL_CheckErrors("Frame: Pre-SCR");
	uint32_t time_elapsed=(uint32_t)(vrFrame.DeltaSeconds*1000.0f);
	resourceManagers.Update(time_elapsed);
	resourceCreator.Update(time_elapsed);
	RenderLocalActors(res);
	GL_CheckErrors("Frame: Post-SCR");

	// Append GuiSys surfaces. This should always be the last item to append the render list.
	mGuiSys->AppendSurfaceList(res.FrameMatrices.CenterView, &res.Surfaces);

	return res;
}

void Application::UpdateHandObjects()
{
	std::vector<ovrTracking> remoteStates;

	uint32_t deviceIndex = 0;
	ovrInputCapabilityHeader capsHeader;
	//Poll controller state from the Oculus API.
	while( vrapi_EnumerateInputDevices(mOvrMobile, deviceIndex, &capsHeader ) >= 0 )
	{
		if(capsHeader.Type == ovrControllerType_TrackedRemote)
		{
			ovrTracking remoteState;
			if(vrapi_GetInputTrackingState(mOvrMobile, capsHeader.DeviceID, 0, &remoteState) >= 0)
			{
				remoteStates.push_back(remoteState);
				if(deviceIndex < 2)
				{
					controllerPoses[deviceIndex].position = *((const avs::vec3*)&remoteState.HeadPose.Pose.Position);
					controllerPoses[deviceIndex].orientation = *((const avs::vec4*)(&remoteState.HeadPose.Pose.Orientation));
				}
				else
				{
					break;
				}
			}
		}
		++deviceIndex;
	}

	OVRActorManager::LiveOVRActor* leftHand = nullptr;
	OVRActorManager::LiveOVRActor* rightHand = nullptr;
    dynamic_cast<OVRActorManager*>(resourceManagers.mActorManager.get())->GetHands(leftHand, rightHand);

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

	OVR::Matrix4f transform;
    scr::mat4 transformToOculusOrigin = scr::mat4::Translation(-oculusOrigin);
    if(rightHand)
    {
        rightHand->actor.UpdateModelMatrix
                (
                        scr::vec3
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
                        rightHand->actor.GetTransform().m_Scale
                );
    }

    if(leftHand)
    {
        leftHand->actor.UpdateModelMatrix
                (
                        scr::vec3
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
                        leftHand->actor.GetTransform().m_Scale
                );
    }
}

bool Application::InitializeController()
{
	ovrInputCapabilityHeader inputCapsHeader;
	for(uint32_t i = 0;
		vrapi_EnumerateInputDevices(mOvrMobile, i, &inputCapsHeader) == 0; ++i) {
		if(inputCapsHeader.Type == ovrControllerType_TrackedRemote)
		{
			if ((int) inputCapsHeader.DeviceID != -1)
			{
				mControllerID = inputCapsHeader.DeviceID;
				OVR_LOG("Found controller (ID: %d)", mControllerID);

				ovrInputTrackedRemoteCapabilities trackedInputCaps;
				trackedInputCaps.Header = inputCapsHeader;
				vrapi_GetInputDeviceCapabilities(mOvrMobile, &trackedInputCaps.Header);
				OVR_LOG("Controller Capabilities: %ud", trackedInputCaps.ControllerCapabilities);
				OVR_LOG("Button Capabilities: %ud", trackedInputCaps.ButtonCapabilities);
				OVR_LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
				OVR_LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
				mTrackpadDim.x = trackedInputCaps.TrackpadMaxX;
				mTrackpadDim.y = trackedInputCaps.TrackpadMaxY;
				return true;
			}
		}
	}

	return false;
}

void Application::OnVideoStreamChanged(const avs::SetupCommand &setupCommand,avs::Handshake &handshake, bool shouldClearEverything, std::vector<avs::uid>& resourcesClientNeeds, std::vector<avs::uid>& outExistingActors)
{
	GlobalGraphicsResources.is_clockwise_winding = setupCommand.is_clockwise_winding;

	if(!mPipelineConfigured)
    {
		OVR_WARN("VIDEO STREAM CHANGED: %d %d %d, cubemap %d", setupCommand.port,
				 setupCommand.video_width, setupCommand.video_height,
				 setupCommand.colour_cubemap_size);

		avs::NetworkSourceParams sourceParams = {};
		sourceParams.socketBufferSize      = 3 * 1024 * 1024; // 3 Mb socket buffer size
		//sourceParams.gcTTL = (1000/60) * 4; // TTL = 4 * expected frame time
		sourceParams.maxJitterBufferLength = 0;


		if (!mNetworkSource.configure(
				NumStreams + (GeoStream ? 1 : 0), setupCommand.port + 1,
				mSession.GetServerIP().c_str(), setupCommand.port, sourceParams))
		{
			OVR_WARN("OnVideoStreamChanged: Failed to configure network source node");
			return;
		}
		mNetworkSource.setDebugStream(setupCommand.debug_stream);
		mNetworkSource.setDebugNetworkPackets(setupCommand.debug_network_packets);
		mNetworkSource.setDoChecksums(setupCommand.do_checksums);
		avs::DecoderParams decoderParams = {};
		decoderParams.codec             = avs::VideoCodec::HEVC;
		decoderParams.decodeFrequency   = avs::DecodeFrequency::NALUnit;
		decoderParams.prependStartCodes = false;
		decoderParams.deferDisplay      = false;
		size_t stream_width  = setupCommand.video_width;
		size_t stream_height = setupCommand.video_height;
		if (!mDecoder.configure(
				avs::DeviceHandle(), stream_width, stream_height, decoderParams, 50))
		{
			OVR_WARN("OnVideoStreamChanged: Failed to configure decoder node");
			mNetworkSource.deconfigure();
			return;
		}
		{
			scr::Texture::TextureCreateInfo textureCreateInfo = {};
			textureCreateInfo.externalResource = true;
			textureCreateInfo.slot   = scr::Texture::Slot::NORMAL;
			textureCreateInfo.format = scr::Texture::Format::RGBA8;
			textureCreateInfo.type   = scr::Texture::Type::TEXTURE_2D_EXTERNAL_OES;
			textureCreateInfo.height = setupCommand.video_height;
			textureCreateInfo.width  = setupCommand.video_width;

			mVideoTexture->Create(textureCreateInfo);
			((scc::GL_Texture *) (mVideoTexture.get()))->SetExternalGlTexture(
					mVideoSurfaceTexture->GetTextureId());

		}
		renderConstants.colourOffsetScale.x = 0;
		renderConstants.colourOffsetScale.y = 0;
		renderConstants.colourOffsetScale.z = 1.0f;
		renderConstants.colourOffsetScale.w =
				float(setupCommand.video_height) / float(stream_height);

		renderConstants.depthOffsetScale.x = 0;
		renderConstants.depthOffsetScale.y =
				float(setupCommand.video_height) / float(stream_height);
		renderConstants.depthOffsetScale.z = float(setupCommand.depth_width) / float(stream_width);
		renderConstants.depthOffsetScale.w =
				float(setupCommand.depth_height) / float(stream_height);

		mSurface.configure(new VideoSurface(mVideoSurfaceTexture));

		mPipeline.link({&mNetworkSource, &mDecoder, &mSurface});

		if (GeoStream)
		{
			avsGeometryDecoder.configure(100, &geometryDecoder);
			avsGeometryTarget.configure(&resourceCreator);
			mPipeline.link({&mNetworkSource, &avsGeometryDecoder, &avsGeometryTarget});
		}
		//GL_CheckErrors("Pre-Build Cubemap");
		//Build Video Cubemap
		{
			scr::Texture::TextureCreateInfo textureCreateInfo =
													{
															setupCommand.colour_cubemap_size,
															setupCommand.colour_cubemap_size,
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
			textureCreateInfo.width  = diffuseSize;
			textureCreateInfo.height = diffuseSize;
			mDiffuseTexture->Create(textureCreateInfo);
			textureCreateInfo.width  = lightSize;
			textureCreateInfo.height = lightSize;
			mCubemapLightingTexture->Create(textureCreateInfo);
			textureCreateInfo.mipCount = 3;
			textureCreateInfo.width  = specularSize;
			textureCreateInfo.height = specularSize;
			mSpecularTexture->Create(textureCreateInfo);
			mRoughSpecularTexture->Create(textureCreateInfo);
			mDiffuseTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
			mSpecularTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
			mRoughSpecularTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
			mCubemapLightingTexture->UseSampler(GlobalGraphicsResources.cubeMipMapSampler);
		}
		//GL_CheckErrors("Built Lighting Cubemap");

		mPipelineConfigured = true;
	}

    if(shouldClearEverything) resourceManagers.Clear();
    else resourceManagers.ClearCareful(resourcesClientNeeds, outExistingActors);

    handshake.startDisplayInfo.width = 1440;
    handshake.startDisplayInfo.height = 1600;
    handshake.framerate = 60;
    handshake.FOV = 110;
    handshake.isVR = true;
	handshake.udpBufferSize = static_cast<uint32_t>(mNetworkSource.getSystemBufferSize());
	handshake.maxBandwidthKpS = 10*handshake.udpBufferSize * static_cast<uint32_t>(handshake.framerate);
	handshake.isReadyToReceivePayloads=true;
	handshake.axesStandard = avs::AxesStandard::GlStyle;
	handshake.MetresPerUnit = 1.0f;
	handshake.usingHands = true;
}

void Application::OnVideoStreamClosed()
{
	OVR_WARN("VIDEO STREAM CLOSED");

	mPipeline.deconfigure();
	mPipeline.reset();
	mPipelineConfigured = false;

    receivedInitialPos = false;
}

bool Application::OnActorEnteredBounds(avs::uid actor_uid)
{
    return resourceManagers.mActorManager->ShowActor(actor_uid);
}

bool Application::OnActorLeftBounds(avs::uid actor_uid)
{
	return resourceManagers.mActorManager->HideActor(actor_uid);
}

void Application::OnFrameAvailable()
{
	++mNumPendingFrames;
}

void Application::avsMessageHandler(avs::LogSeverity severity, const char* msg, void*)
{
	switch(severity)
	{
		case avs::LogSeverity::Error:
		case avs::LogSeverity::Warning:
		if(msg)
		{
			static std::ostringstream ostr;
			while((*msg)!=0&&(*msg)!='\n')
			{
				ostr<<(*msg);
				msg++;
			}
			if(*msg=='\n')
			{
				OVR_WARN("%s", ostr.str().c_str());
				ostr.str("");
				ostr.clear();
			}
			break;
		}
		case avs::LogSeverity::Critical:
			OVR_FAIL("%s", msg);
		default:
			if(msg)
			{
				static std::ostringstream ostr;
				while((*msg)!=0&&(*msg)!='\n')
				{
					ostr<<(*msg);
					msg++;
				}
				if(*msg=='\n')
				{
					OVR_LOG("%s", ostr.str().c_str());
					ostr.str("");
					ostr.clear();
				}
				break;
			}
			break;
	}
}
#include <algorithm>
void Application::CopyToCubemaps()
{
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
		scr::ivec2 offset0={(int32_t) ((3 *  tc.width) / 2),(int32_t) (2 * tc.width)};
		//Lighting Cubemaps
		inputCommand.m_pComputeEffect=mCopyCubemapEffect;
        inputCommand.effectPassName = "CopyCubemap";
		int32_t mip_y=0;
#if 1
		{
			static uint32_t face= 0;
			mip_y = 0;
			int32_t mip_size=diffuseSize;
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

		{
			mip_y = 0;
			int32_t          mip_size = specularSize;
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
		{
			mip_y = 0;
			int32_t          mip_size = specularSize;
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
#else
		cubemapUB.sourceOffset.x+= 3*128;
		{
			mip_y = 0;
			uint32_t      mip_size = 128;
			for (uint32_t m = 0; m < 6; m++)
			{
				inputCommand.m_WorkGroupSize={(mip_size+1)/8,(mip_size+1)/8,6};
				mCubemapComputeShaderResources[0][1].SetImageInfo(0, {GlobalGraphicsResources.cubeMipMapSampler, mSpecularTexture, m});
				cubemapUB.sourceOffset.y       = (int32_t) (2 * tc.width) + mip_y;
				cubemapUB.faceSize = mip_size;
				inputCommand.m_ShaderResources = {mCubemapComputeShaderResources[0][1]};
				mDeviceContext.DispatchCompute(&inputCommand);
				mip_y += 2 * mip_size;
				mip_size /= 2;
			}
		}
		cubemapUB.sourceOffset.x+= 3*128;
		{
			mip_y = 0;
			uint32_t      mip_size = 128;
			for (uint32_t m = 0; m < 6; m++)
			{
				inputCommand.m_WorkGroupSize={(mip_size+1)/8,(mip_size+1)/8,6};
				mCubemapComputeShaderResources[0][1].SetImageInfo(0, {mSampler, mCubemapLightingTexture, m});
				cubemapUB.sourceOffset.y       = (int32_t) (2 * tc.width) + mip_y;
				cubemapUB.faceSize = mip_size;
				inputCommand.m_ShaderResources = {mCubemapComputeShaderResources[0][1]};
				mDeviceContext.DispatchCompute(&inputCommand);
				mip_y += 2 * mip_size;
				mip_size /= 2;
			}
#endif
		GL_CheckErrors("Frame: CopyToCubemaps - Lighting");
		{
            inputCommandCreateInfo.effectPassName = "ExtractPosition";

			scr::uvec3 size  = {1,1,1};
			cubemapUB.sourceOffset		={0,0};
			scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, size, mExtractCameraPositionEffect, {mCubemapComputeShaderResources[0][2]});
			cubemapUB.faceSize			=tc.width;
			cubemapUB.sourceOffset		={(int32_t)mVideoTexture->GetTextureCreateInfo().width - (32 * 4), (int32_t)mVideoTexture->GetTextureCreateInfo().height - (3 * 8)};

			mDeviceContext.DispatchCompute(&inputCommand);
		}

	}
}

void Application::RenderLocalActors(ovrFrameResult& res)
{
	// Because we're using OVR's rendering, we must position the actor's relative to the oculus origin.
	OVR::Matrix4f transform;
	scr::mat4 transformToOculusOrigin = scr::mat4::Translation(-oculusOrigin);

	auto RenderLocalActor = [&](OVRActorManager::LiveOVRActor* ovrActor)
	{
		const scr::Actor& actor = ovrActor->actor;

		//----OVR Actor Set Transforms----//
		scr::mat4 scr_Transform = transformToOculusOrigin * actor.GetTransform().GetTransformMatrix();
		memcpy(&transform.M[0][0], &scr_Transform.a, 16 * sizeof(float));

		for(size_t matIndex = 0; matIndex < actor.GetMaterials().size(); matIndex++)
		{
			if(matIndex >= ovrActor->ovrSurfaceDefs.size())
			{
				//OVR_LOG("Skipping empty element in ovrSurfaceDefs.");
				break;
			}

			res.Surfaces.emplace_back(transform, &ovrActor->ovrSurfaceDefs[matIndex]);
		}
	};

	//Render local actors.
	const std::vector<std::unique_ptr<scr::ActorManager::LiveActor>>& actorList = resourceManagers.mActorManager->GetActorList();
	for(size_t actorIndex = 0; actorIndex < resourceManagers.mActorManager->getVisibleActorAmount(); actorIndex++)
	{
		OVRActorManager::LiveOVRActor* ovrActor = static_cast<OVRActorManager::LiveOVRActor*>(actorList[actorIndex].get());
		RenderLocalActor(ovrActor);
	}

	//Retrieve hands.
	OVRActorManager::LiveOVRActor *leftHand = nullptr, *rightHand = nullptr;
	dynamic_cast<OVRActorManager*>(resourceManagers.mActorManager.get())->GetHands(leftHand, rightHand);

	//Render hands, if they exist.
	if(leftHand) RenderLocalActor(leftHand);
	if(rightHand) RenderLocalActor(rightHand);
}

const scr::Effect::EffectPassCreateInfo& Application::BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl
		, const scr::ShaderSystem::PipelineCreateInfo *pipelineCreateInfo,  const std::vector<scr::ShaderResource>& shaderResources)
{
	if (GlobalGraphicsResources.pbrEffect.HasEffectPass(effectPassName)) return GlobalGraphicsResources.pbrEffect.GetEffectPassCreateInfo(effectPassName);

	scr::ShaderSystem::PassVariables pv;
	pv.mask          = false;
	pv.reverseDepth  = false;
	pv.msaa          = false;

	scr::ShaderSystem::Pipeline gp (&GlobalGraphicsResources.renderPlatform,pipelineCreateInfo);

	//scr::VertexBufferLayout
	vbl->CalculateStride();

	scr::Effect::ViewportAndScissor vs = {};
	vs.x = 0.0f;
	vs.y = 0.0f;
	vs.width = 0.0f;
	vs.height = 0.0f;
	vs.minDepth = 1.0f;
	vs.maxDepth = 0.0f;
	vs.offsetX = 0;
	vs.offsetY = 0;
	vs.extentX = (uint32_t)vs.x;
	vs.extentY = (uint32_t)vs.y;

	scr::Effect::RasterizationState rs = {};
	rs.depthClampEnable = false;
	rs.rasterizerDiscardEnable = false;
	rs.polygonMode = scr::Effect::PolygonMode::FILL;
	rs.cullMode = scr::Effect::CullMode::FRONT_BIT; //As of 2020-02-24, this only affects whether culling is enabled.
	rs.frontFace = scr::Effect::FrontFace::COUNTER_CLOCKWISE; //Unity does clockwise winding, and Unreal does counter-clockwise, but this is set before we connect to a server.

	scr::Effect::MultisamplingState ms = {};
	ms.samplerShadingEnable = false;
	ms.rasterizationSamples = scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT;

	scr::Effect::StencilCompareOpState scos = {};
	scos.stencilFailOp = scr::Effect::StencilCompareOp::KEEP;
	scos.stencilPassDepthFailOp = scr::Effect::StencilCompareOp::KEEP;
	scos.passOp = scr::Effect::StencilCompareOp::KEEP;
	scos.compareOp = scr::Effect::CompareOp::NEVER;
	scr::Effect::DepthStencilingState dss = {};
	dss.depthTestEnable = true;
	dss.depthWriteEnable = true;
	dss.depthCompareOp = scr::Effect::CompareOp::LESS;
	dss.stencilTestEnable = false;
	dss.frontCompareOp = scos;
	dss.backCompareOp = scos;
	dss.depthBoundTestEnable = false;
	dss.minDepthBounds = 0.0f;
	dss.maxDepthBounds = 1.0f;

	scr::Effect::ColourBlendingState cbs = {};
	cbs.blendEnable = true;
	cbs.srcColorBlendFactor = scr::Effect::BlendFactor::SRC_ALPHA;
	cbs.dstColorBlendFactor = scr::Effect::BlendFactor::ONE_MINUS_SRC_ALPHA;
	cbs.colorBlendOp = scr::Effect::BlendOp ::ADD;
	cbs.srcAlphaBlendFactor = scr::Effect::BlendFactor::ONE;
	cbs.dstAlphaBlendFactor = scr::Effect::BlendFactor::ZERO;
	cbs.alphaBlendOp = scr::Effect::BlendOp ::ADD;

	scr::Effect::EffectPassCreateInfo ci;
	ci.effectPassName = effectPassName;
	ci.passVariables = pv;
	ci.pipeline = gp;
	ci.vertexLayout = *vbl;
	ci.topology = scr::Effect::TopologyType::TRIANGLE_LIST;
	ci.viewportAndScissor = vs;
	ci.rasterizationState = rs;
	ci.multisamplingState = ms;
	ci.depthStencilingState = dss;
	ci.colourBlendingState = cbs;

	GlobalGraphicsResources.pbrEffect.CreatePass(&ci);
	GlobalGraphicsResources.pbrEffect.LinkShaders(effectPassName, shaderResources);

	return GlobalGraphicsResources.pbrEffect.GetEffectPassCreateInfo(effectPassName);
}

std::string Application::LoadTextFile(const char *filename)
{
    std::vector<uint8_t> outBuffer;
    std::string str="apk:///assets/";
    str+=filename;
    if(app->GetFileSys().ReadFile(str.c_str(), outBuffer))
    {
        if(outBuffer.back() != '\0')
            outBuffer.push_back('\0'); //Append Null terminator character. ReadFile() does return a null terminated string, apparently!
        return std::string((const char *)outBuffer.data());
    }
    return "";
}