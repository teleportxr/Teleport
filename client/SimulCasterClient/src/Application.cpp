// (C) Copyright 2018-2019 Simul Software Ltd

#include "Application.h"
#include "Config.h"
#include "Input.h"
#include "VideoSurface.h"

#include "GuiSys.h"
#include "OVR_Locale.h"
#include "OVR_LogUtils.h"
#include "OVR_FileSys.h"
#include "OVR_GlUtils.h"
#include "GLESDebug.h"

#include <enet/enet.h>
#include <sstream>

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
	, mOvrMobile(nullptr)
	, mSession(this, resourceCreator)
	, mControllerID(0)
	, mDeviceContext(dynamic_cast<scr::RenderPlatform*>(&renderPlatform))
	, mEffect(dynamic_cast<scr::RenderPlatform*>(&renderPlatform))
{
	pthread_setname_np(pthread_self(), "SimulCaster_Application");
	memset(&renderConstants,0,sizeof(RenderConstants));
	renderConstants.colourOffsetScale={0.0f,0.0f,1.0f,0.6667f};
	renderConstants.depthOffsetScale={0.0f,0.6667f,0.5f,0.3333f};
	mContext.setMessageHandler(Application::avsMessageHandler, this);

	if(enet_initialize() != 0) {
		OVR_FAIL("Failed to initialize ENET library");
	}

	resourceCreator.SetRenderPlatform(dynamic_cast<scr::RenderPlatform*>(&renderPlatform));
	resourceCreator.AssociateResourceManagers(&resourceManagers.mIndexBufferManager, &resourceManagers.mShaderManager, &resourceManagers.mMaterialManager, &resourceManagers.mTextureManager, &resourceManagers.mUniformBufferManager, &resourceManagers.mVertexBufferManager, &resourceManagers.mMeshManager, &resourceManagers.mLightManager);
	resourceCreator.AssociateActorManager(&resourceManagers.mActorManager);

	//Default Effects
	scr::Effect::EffectCreateInfo ci;
	ci.effectName = "StandardEffects";
	mEffect.Create(&ci);

	//Default Sampler
	scr::Sampler::SamplerCreateInfo sci  = {};
	sci.wrapU = scr::Sampler::Wrap::REPEAT;
	sci.wrapV = scr::Sampler::Wrap::REPEAT;
	sci.wrapW = scr::Sampler::Wrap::REPEAT;
	sci.minFilter = scr::Sampler::Filter::MIPMAP_LINEAR;
	sci.magFilter = scr::Sampler::Filter::LINEAR;
	mSampler->Create(&sci);
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
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxFragTextureSlots);
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &maxFragUniformBlocks);
		OVR_LOG("Fragment Texture Slots: %d, Fragment Uniform Blocks: %d", maxFragTextureSlots, maxFragUniformBlocks);

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
			static ovrProgramParm uniformParms[] =    // both TextureMvpProgram and CubeMapPanoProgram use the same parm mapping
										  {
												  {"colourOffsetScale", ovrProgramParmType::FLOAT_VECTOR4},
												  {"depthOffsetScale",  ovrProgramParmType::FLOAT_VECTOR4},
												  {"cubemapTexture", ovrProgramParmType::TEXTURE_SAMPLED},
										  };
			std::string videoSurfaceVert = LoadTextFile("shaders/VideoSurface.vert");
			std::string videoSurfaceFrag = LoadTextFile("shaders/VideoSurface.frag");
			mVideoSurfaceProgram = GlProgram::Build(
					nullptr, videoSurfaceVert.c_str(),
					nullptr, videoSurfaceFrag.c_str(),
					uniformParms, sizeof(uniformParms) / sizeof(ovrProgramParm),
					310);
			if (!mVideoSurfaceProgram.IsValid())
			{
				OVR_FAIL("Failed to build video surface shader program");
			}
		}
		mDecoder.setBackend(new VideoDecoderProxy(java->Env, this, avs::VideoCodec::HEVC));

		mVideoSurfaceTexture = new OVR::SurfaceTexture(java->Env);
		mVideoTexture        = renderPlatform.InstantiateTexture();
		mCubemapUB = renderPlatform.InstantiateUniformBuffer();
		mCubemapUB2 = renderPlatform.InstantiateUniformBuffer();
		{
			scr::Texture::TextureCreateInfo textureCreateInfo={};
			textureCreateInfo.externalResource = true;
			textureCreateInfo.slot=scr::Texture::Slot::NORMAL;
			textureCreateInfo.format=scr::Texture::Format::RGBA8;
			textureCreateInfo.type=scr::Texture::Type::TEXTURE_2D_EXTERNAL_OES;

			mVideoTexture->Create(textureCreateInfo);
			((scc::GL_Texture*)(mVideoTexture.get()))->SetExternalGlTexture(mVideoSurfaceTexture->GetTextureId());

		}
		mCubemapTexture 	    = renderPlatform.InstantiateTexture();
        mDiffuseTexture = renderPlatform.InstantiateTexture();
		mSpecularTexture = renderPlatform.InstantiateTexture();
		mRoughSpecularTexture = renderPlatform.InstantiateTexture();

		mCubemapLightingTexture = renderPlatform.InstantiateTexture();
		{
			CopyCubemapSrc     = LoadTextFile("shaders/CopyCubemap.comp");
			mCopyCubemapEffect = renderPlatform.InstantiateEffect();
			scr::Effect::EffectCreateInfo effectCreateInfo = {};
			effectCreateInfo.effectName = "CopyCubemap";
			mCopyCubemapEffect->Create(&effectCreateInfo);

			scr::ShaderSystem::PipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.m_Count = 1;
			pipelineCreateInfo.m_PipelineType                   = scr::ShaderSystem::PipelineType::PIPELINE_TYPE_COMPUTE;
			pipelineCreateInfo.m_ShaderCreateInfo[0].stage      = scr::Shader::Stage::SHADER_STAGE_COMPUTE;
			pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "main";
			pipelineCreateInfo.m_ShaderCreateInfo[0].filepath   = "shaders/CopyCubemap.comp";
			pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = CopyCubemapSrc;
			scr::ShaderSystem::Pipeline cp(&renderPlatform, &pipelineCreateInfo);


			scr::Effect::EffectPassCreateInfo effectPassCreateInfo;
			effectPassCreateInfo.effectPassName = "CopyCubemap";
			effectPassCreateInfo.pipeline = cp;

			mCopyCubemapEffect->CreatePass(&effectPassCreateInfo);
			{
				scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo = {2, sizeof(CubemapUB), &cubemapUB};
				mCubemapUB->Create(&uniformBufferCreateInfo);
			}
			{
				scr::UniformBuffer::UniformBufferCreateInfo uniformBufferCreateInfo = {2, sizeof(CubemapUB), &cubemapUB2};
				mCubemapUB2->Create(&uniformBufferCreateInfo);
			}
			GL_CheckErrors("mCubemapUB:Create");

            scr::ShaderResourceLayout layout;
            layout.AddBinding(0, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, scr::Shader::Stage ::SHADER_STAGE_COMPUTE);
            layout.AddBinding(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage ::SHADER_STAGE_COMPUTE);
            layout.AddBinding(2, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, scr::Shader::Stage ::SHADER_STAGE_COMPUTE);

            scr::ShaderResource sr({layout, layout});
            sr.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, 0, "destTex", {mSampler, mCubemapTexture,0,uint32_t(-1)});
            sr.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1, "videoFrameTexture", {mSampler, mVideoTexture});
            sr.AddBuffer(0, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB", {mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});

            sr.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE, 0, "destTex", {});
            sr.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 1, "videoFrameTexture", {mSampler, mVideoTexture});
            sr.AddBuffer(1, scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "cubemapUB", {mCubemapUB.get(), 0, mCubemapUB->GetUniformBufferCreateInfo().size});


			mCubemapComputeShaderResources.push_back(sr);

			mCopyCubemapEffect->LinkShaders("CopyCubemap", {});
		}

		mVideoSurfaceDef.surfaceName = "VideoSurface";
		mVideoSurfaceDef.geo = BuildGlobe();//1.f,1.f,1000.f);
		//BBuildTesselatedQuad( 2, 2,true );
		mVideoSurfaceDef.graphicsCommand.Program = mVideoSurfaceProgram;
		mVideoSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
		mVideoSurfaceDef.graphicsCommand.GpuState.cullEnable = false;

		//Set up scr::Camera
		scr::Camera::CameraCreateInfo c_ci = {
				(scr::RenderPlatform*)(&renderPlatform),
				scr::Camera::ProjectionType::PERSPECTIVE,
				scr::quat(1.0f, 0.0f, 0.0f, 0.0f),
				capturePosition
		};
		scrCamera = std::make_shared<scr::Camera>(&c_ci);

		//Set scr::EffectPass
		scr::ShaderSystem::PipelineCreateInfo pipelineCreateInfo;
		{
			pipelineCreateInfo.m_Count                          = 2;
			pipelineCreateInfo.m_PipelineType                   = scr::ShaderSystem::PipelineType::PIPELINE_TYPE_GRAPHICS;
			pipelineCreateInfo.m_ShaderCreateInfo[0].stage      = scr::Shader::Stage::SHADER_STAGE_VERTEX;
			pipelineCreateInfo.m_ShaderCreateInfo[0].entryPoint = "main";
			pipelineCreateInfo.m_ShaderCreateInfo[0].filepath   = "shaders/OpaquePBR.vert";
			pipelineCreateInfo.m_ShaderCreateInfo[0].sourceCode = LoadTextFile("shaders/OpaquePBR.vert");
			pipelineCreateInfo.m_ShaderCreateInfo[1].stage      = scr::Shader::Stage::SHADER_STAGE_FRAGMENT;
			pipelineCreateInfo.m_ShaderCreateInfo[1].entryPoint = "main";
			pipelineCreateInfo.m_ShaderCreateInfo[1].filepath   = "shaders/OpaquePBR.frag";
			pipelineCreateInfo.m_ShaderCreateInfo[1].sourceCode = LoadTextFile("shaders/OpaquePBR.frag");
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
		pbrShaderResource.AddImage(1, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 16, "u_RoughSpecularCubemap", {});

		BuildEffectPass("OpaquePBR", &layout, &pipelineCreateInfo, {pbrShaderResource});

		//Set Lighting Cubemap Shader Resource
        scr::ShaderResourceLayout lightingCubemapLayout;
        lightingCubemapLayout.AddBinding(13, scr::ShaderResourceLayout::ShaderResourceType ::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
        lightingCubemapLayout.AddBinding(14, scr::ShaderResourceLayout::ShaderResourceType ::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		lightingCubemapLayout.AddBinding(15, scr::ShaderResourceLayout::ShaderResourceType ::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
		lightingCubemapLayout.AddBinding(16, scr::ShaderResourceLayout::ShaderResourceType ::COMBINED_IMAGE_SAMPLER, scr::Shader::Stage::SHADER_STAGE_FRAGMENT);

        mLightCubemapShaderResources.SetLayouts({lightingCubemapLayout});
		mLightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 13, "u_DiffuseCubemap", {mDiffuseTexture->GetSampler(), mDiffuseTexture});
		mLightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 14, "u_SpecularCubemap", {mSpecularTexture->GetSampler(), mSpecularTexture});
		mLightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 15, "u_LightsCubemap", {mCubemapLightingTexture->GetSampler(), mCubemapLightingTexture});
		mLightCubemapShaderResources.AddImage(0, scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 16, "u_RoughSpecularCubemap", {mRoughSpecularTexture->GetSampler(), mRoughSpecularTexture});

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
	if(mGuiSys->OnKeyEvent(keyCode, repeatCount, eventType))
	{
		return true;
	}
	return false;
}

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
			controllerState.mTrackpadStatus = ovrState.TrackpadStatus > 0;
			controllerState.mTrackpadX = ovrState.TrackpadPosition.x / mTrackpadDim.x;
			controllerState.mTrackpadY = ovrState.TrackpadPosition.y / mTrackpadDim.y;
			controllerState.mJoystickAxisX=ovrState.Joystick.x;
			controllerState.mJoystickAxisY=ovrState.Joystick.y;
		}
	}

	// Handle networked session.
	if(mSession.IsConnected())
	{
		mSession.Frame(vrFrame, controllerState);
	}
	else
	{
		ENetAddress remoteEndpoint;
		if(mSession.Discover(REMOTEPLAY_DISCOVERY_PORT, remoteEndpoint))
		{
			mSession.Connect(remoteEndpoint, REMOTEPLAY_TIMEOUT);
		}
	}

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

	//Get the Capture Position
	scr::Transform::TransformCreateInfo tci = {(scr::RenderPlatform*)(&renderPlatform)};
	scr::Transform scr_UE4_captureTransform(tci);
	avs::Transform avs_UE4_captureTransform = mDecoder.getCameraTransform();
	scr_UE4_captureTransform = avs_UE4_captureTransform;
	capturePosition = scr_UE4_captureTransform.m_Translation;
	scrCamera->UpdatePosition(capturePosition);

	static float frameRate=1.0f;
	if(vrFrame.DeltaSeconds>0.0f)
	{
		frameRate*=0.99f;
		frameRate+=0.01f/vrFrame.DeltaSeconds;
	}
#if 1
	ovrQuatf headPose = vrFrame.Tracking.HeadPose.Pose.Orientation;
	ovrVector3f headPos=vrFrame.Tracking.HeadPose.Pose.Position;
	auto ctr=mNetworkSource.getCounterValues();
	mGuiSys->ShowInfoText( 0.017f,"Packets Dropped: Network %d | Decoder %d\n Framerate: %4.4f Bandwidth(kbps): %4.4f\n Actors: SCR %d | OVR %d | Lights: %d\n Capture Position: %1.3f, %1.3f, %1.3f\n"
							 "Orient: %1.3f, {%1.3f, %1.3f, %1.3f}\nPos: %3.3f %3.3f %3.3f \nTrackpad: %3.1f %3.1f | Orphans: %d\n"
			, ctr.networkPacketsDropped, ctr.decoderPacketsDropped,
			frameRate, ctr.bandwidthKPS,
			(uint64_t)resourceManagers.mActorManager.GetActorList().size(), (uint64_t)mOVRActors.size(), resourceManagers.mLightManager.GetCache().size(),
			capturePosition.x, capturePosition.y, capturePosition.z,
			headPose.w, headPose.x, headPose.y, headPose.z,
			headPos.x,headPos.y,headPos.z,
			controllerState.mTrackpadX,controllerState.mTrackpadY,
			ctr.m_packetMapOrphans);

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
	mVideoSurfaceDef.graphicsCommand.UniformData[0].Data = &renderConstants.colourOffsetScale;
	mVideoSurfaceDef.graphicsCommand.UniformData[1].Data = &renderConstants.depthOffsetScale;
	if(mCubemapTexture->IsValid())
	{
		mVideoSurfaceDef.graphicsCommand.UniformData[2].Data = &(((scc::GL_Texture *) mCubemapTexture.get())->GetGlTexture());
		res.Surfaces.push_back(ovrDrawSurface(&mVideoSurfaceDef));
	}

	//Move the hands before they are drawn.
    {
        std::vector<ovrTracking> remoteStates;

        uint32_t deviceIndex = 0;
        ovrInputCapabilityHeader capsHeader;
        //Poll controller state from the Oculus API.
        while( vrapi_EnumerateInputDevices(mOvrMobile, deviceIndex, &capsHeader ) >= 0 )
        {
            if ( capsHeader.Type == ovrControllerType_TrackedRemote )
            {
                ovrTracking remoteState;
                if(vrapi_GetInputTrackingState(mOvrMobile, capsHeader.DeviceID, 0, &remoteState) >= 0)
                {
                    remoteStates.push_back(remoteState);
                }
            }

            ++deviceIndex;
        }

        size_t handIndex = 0;
        //Update hands to current position, and orientation.
        for(avs::uid handID : resourceManagers.mActorManager.handUIDs)
        {
            std::shared_ptr<scr::Actor> hand = resourceManagers.mActorManager.GetActor(handID);

            //Break if the client doesn't have the hand actor yet.
            if(!hand)
            {
                break;
            }

            //Hands are only visible, if there is a hand for them to position relative to.
            if(handIndex > remoteStates.size())
            {
                hand->isVisible = false;
                continue;
            }
            hand->isVisible = true;

            hand->UpdateModelMatrix
            (
               scr::vec3
               {
                   remoteStates[handIndex].HeadPose.Pose.Position.x + headPos.x,
                   remoteStates[handIndex].HeadPose.Pose.Position.y + headPos.y,
                   remoteStates[handIndex].HeadPose.Pose.Position.z + headPos.z
               },
               scr::quat
               {
                   remoteStates[handIndex].HeadPose.Pose.Orientation.w,
                   remoteStates[handIndex].HeadPose.Pose.Orientation.x,
                   remoteStates[handIndex].HeadPose.Pose.Orientation.y,
                   remoteStates[handIndex].HeadPose.Pose.Orientation.z
               }
               * HAND_ROTATION_DIFFERENCE,
               hand->GetTransform().m_Scale
            );

            ++handIndex;
        }
    }

	//Append SCR Actors to surfaces.
	GL_CheckErrors("Frame: Pre-SCR");
	RemoveInvalidOVRActors();
	uint32_t time_elapsed=(uint32_t)(vrFrame.DeltaSeconds*1000.0f);
	resourceManagers.Update(time_elapsed);
	RenderLocalActors(res);
	GL_CheckErrors("Frame: Post-SCR");

	// Append GuiSys surfaces. This should always be the last item to append the render list.
	mGuiSys->AppendSurfaceList(res.FrameMatrices.CenterView, &res.Surfaces);

	return res;
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

void Application::OnVideoStreamChanged(const avs::SetupCommand &setupCommand,avs::Handshake &handshake)
{
	if(mPipelineConfigured) {
		// TODO: Fix!
		return;
	}

	OVR_WARN("VIDEO STREAM CHANGED: %d %d %d", setupCommand.port, setupCommand.video_width, setupCommand.video_height);

	avs::NetworkSourceParams sourceParams = {};
	sourceParams.socketBufferSize = 3 * 1024 * 1024; // 4* 64MiB socket buffer size
	//sourceParams.gcTTL = (1000/60) * 4; // TTL = 4 * expected frame time
	sourceParams.maxJitterBufferLength = 0;


	if(!mNetworkSource.configure(NumStreams + (GeoStream?1:0), setupCommand.port+1, mSession.GetServerIP().c_str(), setupCommand.port, sourceParams)) {
		OVR_WARN("OnVideoStreamChanged: Failed to configure network source node");
		return;
	}
	mNetworkSource.setDebugStream(setupCommand.debug_stream);
	mNetworkSource.setDoChecksums(setupCommand.do_checksums);
	avs::DecoderParams decoderParams = {};
	decoderParams.codec = avs::VideoCodec::HEVC;
	decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
	decoderParams.prependStartCodes = false;
	decoderParams.deferDisplay = false;
	size_t stream_width=setupCommand.video_width;
	size_t stream_height=setupCommand.video_height;
	if(!mDecoder.configure(avs::DeviceHandle(), stream_width, stream_height, decoderParams, 50))
	{
		OVR_WARN("OnVideoStreamChanged: Failed to configure decoder node");
		mNetworkSource.deconfigure();
		return;
	}

	renderConstants.colourOffsetScale.x =0;
	renderConstants.colourOffsetScale.y = 0;
	renderConstants.colourOffsetScale.z = 1.0f;
	renderConstants.colourOffsetScale.w = float(setupCommand.video_height) / float(stream_height);

	renderConstants.depthOffsetScale.x = 0;
	renderConstants.depthOffsetScale.y = float(setupCommand.video_height) / float(stream_height);
	renderConstants.depthOffsetScale.z = float(setupCommand.depth_width) / float(stream_width);
	renderConstants.depthOffsetScale.w = float(setupCommand.depth_height) / float(stream_height);

	mSurface.configure(new VideoSurface(mVideoSurfaceTexture));

	mPipeline.link({&mNetworkSource, &mDecoder, &mSurface});

   if(GeoStream)
   {
		avsGeometryDecoder.configure(100, &geometryDecoder);
		avsGeometryTarget.configure(&resourceCreator);
		mPipeline.link({ &mNetworkSource, &avsGeometryDecoder, &avsGeometryTarget });
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
	   mCubemapTexture->UseSampler(mSampler);
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
		textureCreateInfo.mipCount=1;
		mDiffuseTexture->Create(textureCreateInfo);
		mCubemapLightingTexture->Create(textureCreateInfo);
		textureCreateInfo.mipCount=3;
		mSpecularTexture->Create(textureCreateInfo);
		mRoughSpecularTexture->Create(textureCreateInfo);
		mDiffuseTexture->UseSampler(mSampler);
		mSpecularTexture->UseSampler(mSampler);
		mRoughSpecularTexture->UseSampler(mSampler);
		mCubemapLightingTexture->UseSampler(mSampler);
	}
    //GL_CheckErrors("Built Lighting Cubemap");

	mPipelineConfigured = true;

	handshake.framerate=60;
	handshake.udpBufferSize=mNetworkSource.getSystemBufferSize();
	handshake.maxBandwidth=handshake.udpBufferSize*(size_t)handshake.framerate;
}

void Application::OnVideoStreamClosed()
{
	OVR_WARN("VIDEO STREAM CLOSED");

	mPipeline.deconfigure();
	mPipeline.reset();
	mPipelineConfigured = false;
}

bool Application::OnActorEnteredBounds(avs::uid actor_uid)
{
    return resourceManagers.mActorManager.ShowActor(actor_uid);
}

bool Application::OnActorLeftBounds(avs::uid actor_uid)
{
    return resourceManagers.mActorManager.HideActor(actor_uid);
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

		scr::InputCommandCreateInfo inputCommandCreateInfo={};
		scr::InputCommand_Compute inputCommand(&inputCommandCreateInfo, size, mCopyCubemapEffect, {mCubemapComputeShaderResources[0][0]});
		cubemapUB.faceSize=tc.width;
		cubemapUB.sourceOffset={0,0};
		cubemapUB.mip             = 0;
		cubemapUB.face             = 0;

		mDeviceContext.DispatchCompute(&inputCommand);
		GL_CheckErrors("Frame: CopyToCubemaps - Main");
		cubemapUB.faceSize = 128;
		cubemapUB.sourceOffset.x= (int32_t) ((3 *  tc.width) / 2);
		//Lighting Cubemaps
		uint32_t mip_y=0;
#if 1
		{
			static uint32_t face= 0;
			mip_y = 0;
			uint32_t         mip_size = 128;
			uint32_t M=mDiffuseTexture->GetTextureCreateInfo().mipCount;
			for (uint32_t m        = 0; m < M; m++)
			{
					inputCommand.m_WorkGroupSize = {(mip_size + 1) / ThreadCount, (mip_size + 1) / ThreadCount ,6};
					mCubemapComputeShaderResources[0].SetImageInfo(1, 0, {
							mDiffuseTexture->GetSampler(), mDiffuseTexture, m});
					cubemapUB.sourceOffset.y       = (int32_t) (2 * tc.width) + mip_y;
					cubemapUB.faceSize             = mip_size;
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
		cubemapUB.sourceOffset.x+= 3*128;
		{
			mip_y=0;
			uint32_t mip_size=128;
			uint32_t M=mSpecularTexture->GetTextureCreateInfo().mipCount;
			for(uint32_t m=0;m<M;m++)
			{
				inputCommand.m_WorkGroupSize={(mip_size+1)/ThreadCount,(mip_size+1)/ThreadCount,6};
				mCubemapComputeShaderResources[0].SetImageInfo(1 ,0, {mSpecularTexture->GetSampler(), mSpecularTexture, m});
				cubemapUB.sourceOffset.y = (int32_t) (2 *  tc.width) + mip_y;
				cubemapUB.faceSize = mip_size;
				cubemapUB.mip             = m;
				cubemapUB.face				   = 0;
				inputCommand.m_ShaderResources = {mCubemapComputeShaderResources[0][1]};
				mDeviceContext.DispatchCompute(&inputCommand);
				mip_y+=2*mip_size;
				mip_size/=2;
			}
		}
		cubemapUB.sourceOffset.x+= 3*128;
		{
			mip_y=0;
			uint32_t mip_size=128;
			uint32_t M=mRoughSpecularTexture->GetTextureCreateInfo().mipCount;
			for(uint32_t m=0;m<M;m++)
			{
				inputCommand.m_WorkGroupSize={(mip_size+1)/ThreadCount,(mip_size+1)/ThreadCount,6};
				mCubemapComputeShaderResources[0].SetImageInfo(1 ,0, {mRoughSpecularTexture->GetSampler(), mRoughSpecularTexture, m});
				cubemapUB.sourceOffset.y = (int32_t) (2 *  tc.width) + mip_y;
				cubemapUB.faceSize = mip_size;
				cubemapUB.mip             = m;
				cubemapUB.face				   = 0;
				inputCommand.m_ShaderResources = {mCubemapComputeShaderResources[0][1]};
				mDeviceContext.DispatchCompute(&inputCommand);
				mip_y+=2*mip_size;
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
				mCubemapComputeShaderResources[0][1].SetImageInfo(0, {mSampler, mSpecularTexture, m});
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
		}
#endif
		GL_CheckErrors("Frame: CopyToCubemaps - Lighting");
	}
}

void Application::RenderLocalActors(ovrFrameResult& res)
{
	scr::InputCommandCreateInfo ci;
	ci.type = scr::INPUT_COMMAND_MESH_MATERIAL_TRANSFORM;
	ci.pFBs = nullptr;
	ci.frameBufferCount = 0;
	ci.pCamera = scrCamera.get();
	for(auto& a : resourceManagers.mActorManager.GetActorList())
	{
		auto &liveActor=a.second;
		auto &actor=liveActor.actor;
		if(!actor->isVisible)
		{
            continue;
        }
		size_t num_elements=actor->GetMaterials().size();

		if(mOVRActors.find(a.first) == mOVRActors.end())
		{
			mOVRActors[a.first]; //Create
			std::shared_ptr<OVRActor> pOvrActor = std::make_shared<OVRActor>();
			//pOvrActor->ovrSurfaceDefs.reserve(num_elements);

			for(size_t i=0;i<num_elements;i++)
			{
				//From Actor
				const scr::Mesh::MeshCreateInfo   &meshCI     = actor->GetMesh()->GetMeshCreateInfo();
				scr::Material::MaterialCreateInfo &materialCI = actor->GetMaterials()[i]->GetMaterialCreateInfo();
				if(i>=meshCI.vb.size()||i>=meshCI.ib.size())
				{
					OVR_LOG("Skipping empty element in mesh.");
					break;
				}
				//Mesh.
				// The first instance of vb/ib should be adequate to get the information needed.
				const auto gl_vb = dynamic_cast<scc::GL_VertexBuffer *>(meshCI.vb[i].get());
				const auto gl_ib = dynamic_cast<scc::GL_IndexBuffer *>(meshCI.ib[i].get());
				gl_vb->CreateVAO(gl_ib->GetIndexID());

				//Material
				std::vector<scr::ShaderResource> pbrShaderResources;
				pbrShaderResources.push_back(ci.pCamera->GetShaderResource());
				pbrShaderResources.push_back(actor->GetMaterials()[i]->GetShaderResource());
				pbrShaderResources.push_back(mLightCubemapShaderResources);

				materialCI.effect = dynamic_cast<scr::Effect *>(&mEffect);
				const auto                            gl_effect = &mEffect;
				const auto gl_effectPass = gl_effect->GetEffectPassCreateInfo("OpaquePBR");
				if(materialCI.diffuse.texture)
					materialCI.diffuse.texture->UseSampler(mSampler);
				if(materialCI.normal.texture)
					materialCI.normal.texture->UseSampler(mSampler);
				if(materialCI.combined.texture)
					materialCI.combined.texture->UseSampler(mSampler);

				//----Set OVR Actor----//
				//Construct Mesh
				GlGeometry geo;
				geo.vertexBuffer      = gl_vb->GetVertexID();
				geo.indexBuffer       = gl_ib->GetIndexID();
				geo.vertexArrayObject = gl_vb->GetVertexArrayID();
				geo.primitiveType     = scc::GL_Effect::ToGLTopology(gl_effectPass.topology);
				geo.vertexCount       = (int) gl_vb->GetVertexCount();
				geo.indexCount        = (int) gl_ib->GetIndexBufferCreateInfo().indexCount;
				GlGeometry::IndexType = gl_ib->GetIndexBufferCreateInfo().stride == 4 ? GL_UNSIGNED_INT : gl_ib->GetIndexBufferCreateInfo().stride == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

				//Initialise OVR Actor
				std::shared_ptr<ovrSurfaceDef> ovr_surface_def(new ovrSurfaceDef);
				std::string   _actorName = std::string("ActorUID: ") + std::to_string(a.first);
				ovr_surface_def->surfaceName  = _actorName;
				ovr_surface_def->numInstances = 1;
				ovr_surface_def->geo          = geo;

				//Set Shader Program
				ovr_surface_def->graphicsCommand.Program = gl_effect->GetGlPlatform();

				//Set Rendering Set
				ovr_surface_def->graphicsCommand.GpuState.blendMode			= scc::GL_Effect::ToGLBlendOp(gl_effectPass.colourBlendingState.colorBlendOp);
				ovr_surface_def->graphicsCommand.GpuState.blendSrc			= scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.srcColorBlendFactor);
				ovr_surface_def->graphicsCommand.GpuState.blendDst			= scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.dstColorBlendFactor);
				ovr_surface_def->graphicsCommand.GpuState.blendModeAlpha	= scc::GL_Effect::ToGLBlendOp(gl_effectPass.colourBlendingState.alphaBlendOp);
				ovr_surface_def->graphicsCommand.GpuState.blendSrcAlpha		= scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.srcAlphaBlendFactor);
				ovr_surface_def->graphicsCommand.GpuState.blendDstAlpha		= scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.dstAlphaBlendFactor);
				ovr_surface_def->graphicsCommand.GpuState.depthFunc			= scc::GL_Effect::ToGLCompareOp(gl_effectPass.depthStencilingState.depthCompareOp);

				ovr_surface_def->graphicsCommand.GpuState.frontFace		= gl_effectPass.rasterizationState.frontFace == scr::Effect::FrontFace::COUNTER_CLOCKWISE ? GL_CCW : GL_CW;
				ovr_surface_def->graphicsCommand.GpuState.polygonMode	= scc::GL_Effect::ToGLPolygonMode(gl_effectPass.rasterizationState.polygonMode);
				ovr_surface_def->graphicsCommand.GpuState.blendEnable	= gl_effectPass.colourBlendingState.blendEnable ? OVR::ovrGpuState::ovrBlendEnable::BLEND_ENABLE: OVR::ovrGpuState::ovrBlendEnable::BLEND_DISABLE;
				ovr_surface_def->graphicsCommand.GpuState.depthEnable     = gl_effectPass.depthStencilingState.depthTestEnable;
				ovr_surface_def->graphicsCommand.GpuState.depthMaskEnable = true;
				ovr_surface_def->graphicsCommand.GpuState.colorMaskEnable[0] = true;
				ovr_surface_def->graphicsCommand.GpuState.colorMaskEnable[1] = true;
				ovr_surface_def->graphicsCommand.GpuState.colorMaskEnable[2] = true;
				ovr_surface_def->graphicsCommand.GpuState.colorMaskEnable[3] = true;
				ovr_surface_def->graphicsCommand.GpuState.polygonOffsetEnable = false;
				ovr_surface_def->graphicsCommand.GpuState.cullEnable = gl_effectPass.rasterizationState.cullMode == scr::Effect::CullMode::NONE ? false : true;
				ovr_surface_def->graphicsCommand.GpuState.lineWidth = 1.0F;
				ovr_surface_def->graphicsCommand.GpuState.depthRange[0] = gl_effectPass.depthStencilingState.minDepthBounds;
				ovr_surface_def->graphicsCommand.GpuState.depthRange[1] = gl_effectPass.depthStencilingState.maxDepthBounds;

				//Update Uniforms and Textures
           		size_t resourceCount = 0;
           		GLint textureCount = 0, uniformCount = 0;
				size_t j=0;
				for (auto &sr : pbrShaderResources)
				{
					std::vector<scr::ShaderResource::WriteShaderResource> &shaderResourceSet=sr.GetWriteShaderResources();
					for (auto &resource : shaderResourceSet)
					{
						scr::ShaderResourceLayout::ShaderResourceType type = resource.shaderResourceType;
						if (type == scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER)
						{
							if (resource.imageInfo.texture.get())
							{
								auto gl_texture = dynamic_cast<scc::GL_Texture *>(resource.imageInfo.texture.get());
								ovr_surface_def->graphicsCommand.UniformData[j].Data = &(gl_texture->GetGlTexture());
								textureCount++;
							}
						}
						else if (type == scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER)
						{
							if (resource.bufferInfo.buffer)
							{
								auto gl_uniformBuffer = (scc::GL_UniformBuffer *) (resource.bufferInfo.buffer);
								gl_uniformBuffer->Submit();
								ovr_surface_def->graphicsCommand.UniformData[j].Data = &(gl_uniformBuffer->GetGlBuffer());
								uniformCount++;
							}
						}
						else
						{
							//NULL
						}
						j++;
						resourceCount++;
						assert(resourceCount <= OVR::ovrUniform::MAX_UNIFORMS);
						assert(textureCount <= maxFragTextureSlots);
						assert(uniformCount <= maxFragUniformBlocks);
					}
				}
				pOvrActor->ovrSurfaceDefs.push_back(ovr_surface_def);
			}
            mOVRActors[a.first] = pOvrActor; //Assign
		}

		//The OVR actor has been found/created.
		// Now update its transform:
		std::shared_ptr<OVRActor> pOvrActor=mOVRActors[a.first];
		assert(pOvrActor);
		for(size_t i=0;i<num_elements;i++)
		{
			if(i>=pOvrActor->ovrSurfaceDefs.size())
			{
				OVR_LOG("Skipping empty element in ovrSurfaceDefs.");
				break;
			}
			OVR_LOG("BlendDst is %d",(int)pOvrActor->ovrSurfaceDefs[i]->graphicsCommand.GpuState.blendDst);
			//----OVR Actor Set Transforms----//
			float     heightOffset = -0.0F;
			scr::vec3 camPos       = capturePosition * -1;
			camPos.y += heightOffset;

			//Change of Basis matrix
			scr::mat4 cob = scr::mat4({0, 1, 0, 0}, {0, 0, 1, 0}, {-1, 0, 0, 0}, {0, 0, 0, 1});
			scr::mat4 inv_ue4ViewMatrix = scr::mat4::Translation(camPos);
			scr::mat4 scr_Transform = inv_ue4ViewMatrix * actor->GetTransform().GetTransformMatrix() * cob;

			OVR::Matrix4f transform;
			memcpy(&transform.M[0][0], &scr_Transform.a, 16 * sizeof(float));
			res.Surfaces.emplace_back(transform, pOvrActor->ovrSurfaceDefs[i].get());
		}
	}
}

const scr::Effect::EffectPassCreateInfo& Application::BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl
		, const scr::ShaderSystem::PipelineCreateInfo *pipelineCreateInfo,  const std::vector<scr::ShaderResource>& shaderResources)
{
	if (mEffect.HasEffectPass(effectPassName))
		return mEffect.GetEffectPassCreateInfo(effectPassName);

	scr::ShaderSystem::PassVariables pv;
	pv.mask          = false;
	pv.reverseDepth  = false;
	pv.msaa          = false;

	scr::ShaderSystem::Pipeline gp (&renderPlatform,pipelineCreateInfo);

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
	rs.cullMode = scr::Effect::CullMode::BACK_BIT;
	rs.frontFace = scr::Effect::FrontFace::CLOCKWISE;

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

    mEffect.CreatePass(&ci);

    mEffect.LinkShaders(effectPassName, shaderResources);

	return mEffect.GetEffectPassCreateInfo(effectPassName);
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