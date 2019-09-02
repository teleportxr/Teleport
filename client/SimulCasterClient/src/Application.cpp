// (C) Copyright 2018-2019 Simul Software Ltd

#include "Application.h"
#include "Config.h"
#include "Input.h"
#include "VideoSurface.h"

#include "GuiSys.h"
#include "OVR_Locale.h"
#include "GLSLShaders.h"
#include "OVR_LogUtils.h"

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
	, mOvrMobile(nullptr)
	, mSession(this)
	, mControllerID(-1)
	, mDeviceContext(dynamic_cast<scr::RenderPlatform*>(&renderPlatform))
	, mFlatColourEffect(dynamic_cast<scr::RenderPlatform*>(&renderPlatform))
{
	memset(&renderConstants,0,sizeof(RenderConstants));
	renderConstants.colourOffsetScale={0.0f,0.0f,1.0f,0.6667f};
	renderConstants.depthOffsetScale={0.0f,0.6667f,0.5f,0.3333f};
	mContext.setMessageHandler(Application::avsMessageHandler, this);

	if(enet_initialize() != 0) {
		OVR_FAIL("Failed to initialize ENET library");
	}

	resourceCreator.SetRenderPlatform(dynamic_cast<scr::RenderPlatform*>(&renderPlatform));
	resourceCreator.AssociateResourceManagers(&resourceManagers.mIndexBufferManager, &resourceManagers.mShaderManager, &resourceManagers.mMaterialManager, &resourceManagers.mTextureManager, &resourceManagers.mUniformBufferManager, &resourceManagers.mVertexBufferManager);
	resourceCreator.AssociateActorManager(&resourceManagers.mActorManager);

	scr::Effect::EffectCreateInfo ci;
	ci.effectName = "flatColour";
	mFlatColourEffect.Create(&ci);
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
		const ovrJava* java = app->GetJava();

		mOvrMobile=app->GetOvrMobile();

		mSoundEffectContext = new ovrSoundEffectContext(*java->Env, java->ActivityObject);
		mSoundEffectContext->Initialize(&app->GetFileSys());
		mSoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

		mLocale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");

		std::string fontName;
		GetLocale().GetString("@string/font_name", "efigs.fnt", fontName);
		mGuiSys->Init(this->app, *mSoundEffectPlayer, fontName.c_str(), &app->GetDebugLines());

		//VideoSurfaceProgram
		{
			static ovrProgramParm uniformParms[] =	// both TextureMvpProgram and CubeMapPanoProgram use the same parm mapping
										  {
												  { "colourOffsetScale",	ovrProgramParmType::FLOAT_VECTOR4 },
												  { "depthOffsetScale",		ovrProgramParmType::FLOAT_VECTOR4 },
												  { "videoFrameTexture",	ovrProgramParmType::TEXTURE_SAMPLED },
										  };
			mVideoSurfaceProgram = GlProgram::Build(nullptr, shaders::VideoSurface_VS,
													shaders::VideoSurface_OPTIONS, shaders::VideoSurface_FS,
													uniformParms, sizeof( uniformParms ) / sizeof( ovrProgramParm ),
													310);
			if(!mVideoSurfaceProgram.IsValid()) {
				OVR_FAIL("Failed to build video surface shader program");
			}
		}
		//FlatColourEffect and Material
		{
			scr::ShaderSystem::PassVariables pv;
			pv.mask = false;
			pv.reverseDepth = false;
			pv.msaa = false;

			scc::GL_Shader shaders[2] = {
					scc::GL_Shader(dynamic_cast<scr::RenderPlatform*>(&renderPlatform)),
					scc::GL_Shader(dynamic_cast<scr::RenderPlatform*>(&renderPlatform))};
			scr::Shader::ShaderCreateInfo sci[2];
			sci[0].stage = scr::Shader::Stage::SHADER_STAGE_VERTEX;
			sci[0].entryPoint = "main";
			sci[0].filepath = nullptr;
			sci[0].sourceCode = shaders::FlatTexture_VS;
			sci[1].stage = scr::Shader::Stage::SHADER_STAGE_FRAGMENT;
			sci[1].entryPoint = "main";
			sci[1].filepath = nullptr;
			sci[1].sourceCode = shaders::FlatTexture_FS;
			shaders[0].Create(&sci[0]);
			shaders[1].Create(&sci[1]);
			scr::ShaderSystem::GraphicsPipeline gp (shaders, 2);

			scr::VertexBufferLayout vbl;
			vbl.AddAttribute(0, scr::VertexBufferLayout::ComponentCount::VEC4, scr::VertexBufferLayout::Type::FLOAT);
			vbl.AddAttribute(3, scr::VertexBufferLayout::ComponentCount::VEC2, scr::VertexBufferLayout::Type::FLOAT);
			vbl.AddAttribute(4, scr::VertexBufferLayout::ComponentCount::VEC2, scr::VertexBufferLayout::Type::FLOAT);
			vbl.CalculateStride();

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
			dss.depthWriteEnable = false;
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
			ci.effectPassName = "standard";
			ci.passVariables = pv;
			ci.pipeline = gp;
			ci.vertexLayout = vbl;
			ci.topology = scr::Effect::TopologyType::TRIANGLE_LIST;
			ci.viewportAndScissor = vs;
			ci.rasterizationState = rs;
			ci.multisamplingState = ms;
			ci.depthStencilingState = dss;
			ci.colourBlendingState = cbs;
			mFlatColourEffect.CreatePass(&ci);
			mFlatColourEffect.LinkShaders("standard");

			scr::Texture::TextureCreateInfo t_ci = {};
			t_ci.width = 1;
			t_ci.height = 1;
			t_ci.depth = 1;
			t_ci.bytesPerPixel = 4;
			t_ci.arrayCount = 1;
			t_ci.mipCount = 1;
			t_ci.slot = scr::Texture::Slot ::DIFFUSE;
			t_ci.type = scr::Texture::Type ::TEXTURE_2D;
			t_ci.format = scr::Texture::Format ::RGBA8;
			t_ci.sampleCount = scr::Texture::SampleCountBit ::SAMPLE_COUNT_1_BIT;
			t_ci.size = 0;
			t_ci.data = nullptr;
			mDummyTexture = std::make_shared<scc::GL_Texture>(&renderPlatform);
			mDummyTexture->Create(&t_ci);
			scr::Material::MaterialParameter mp;
			mp.texture.reset(mDummyTexture.get());
			mp.textureOutputScalar = scr::vec4(1.0f, 1.0f, 1.0f, 1.0f);
			mp.texCoordsScalar[0] = scr::vec2(1.0f, 1.0f);
			mp.texCoordsScalar[1] = scr::vec2(1.0f, 1.0f);
			mp.texCoordsScalar[2] = scr::vec2(1.0f, 1.0f);
			mp.texCoordsScalar[3] = scr::vec2(1.0f, 1.0f);
			scr::Material::MaterialCreateInfo m_ci;
			m_ci.effect = dynamic_cast<scr::Effect*>(&mFlatColourEffect);
			m_ci.diffuse = mp;
			m_ci.normal = mp;
			m_ci.combined = mp;
			mFlatColourMaterial = std::make_shared<scr::Material>(&renderPlatform,&m_ci);
		}

		mDecoder.setBackend(new VideoDecoderProxy(java->Env, this, avs::VideoCodec::HEVC));

		mVideoSurfaceTexture = new OVR::SurfaceTexture(java->Env);
		mVideoTexture = GlTexture(mVideoSurfaceTexture->GetTextureId(), GL_TEXTURE_EXTERNAL_OES, 0, 0);

		mVideoSurfaceDef.surfaceName = "VideoSurface";
		mVideoSurfaceDef.geo = BuildGlobe();
		mVideoSurfaceDef.graphicsCommand.Program = mVideoSurfaceProgram;
		mVideoSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
		mVideoSurfaceDef.graphicsCommand.GpuState.cullEnable = false;

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
	if((int)mControllerID == -1) {
		InitializeController();
	}

	// Query controller input state.
	ControllerState controllerState = {};
	if((int)mControllerID != -1)
	{
		ovrInputStateTrackedRemote ovrState;
		ovrState.Header.ControllerType = ovrControllerType_TrackedRemote;
		if(vrapi_GetCurrentInputState(mOvrMobile, mControllerID, &ovrState.Header) >= 0)
		{
			controllerState.mButtons = ovrState.Buttons;
			controllerState.mTrackpadStatus = ovrState.TrackpadStatus > 0;
			controllerState.mTrackpadX = ovrState.TrackpadPosition.x / mTrackpadDim.x;
			controllerState.mTrackpadY = ovrState.TrackpadPosition.y / mTrackpadDim.y;
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

	static float frameRate=1.0f;
	if(vrFrame.DeltaSeconds>0.0f)
	{
		frameRate*=0.99f;
		frameRate+=0.01f/vrFrame.DeltaSeconds;
	}
#if 1
	auto ctr=mNetworkSource.getCounterValues();
	mGuiSys->ShowInfoText( 1.0f , "Network Packets Dropped: %d\n Decoder Packets Dropped: %d\n Framerate: %4.4f\n Bandwidth(kbps): %4.4f\n Actors: SCR %d | OVR %d"
			, ctr.networkPacketsDropped, ctr.decoderPacketsDropped
			,frameRate,ctr.bandwidthKPS, (uint64_t)resourceManagers.mActorManager.m_Actors.size(), (uint64_t)mOVRActors.size());
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

	// Append video surface
	mVideoSurfaceDef.graphicsCommand.UniformData[0].Data = &renderConstants.colourOffsetScale;
	mVideoSurfaceDef.graphicsCommand.UniformData[1].Data = &renderConstants.depthOffsetScale;
	mVideoSurfaceDef.graphicsCommand.UniformData[2].Data = &mVideoTexture;
	res.Surfaces.push_back(ovrDrawSurface(&mVideoSurfaceDef));

	// Append GuiSys surfaces.
	mGuiSys->AppendSurfaceList(res.FrameMatrices.CenterView, &res.Surfaces);


	//Append SCR Actors to surfaces.
	GL_CheckErrors("Frame: Pre-SCR");
	//Remove Invalid scr and ovr actors.
		//mActorManager.RemoveInvalidActors();
		//RemoveInvalidOVRActors();
	RenderLocalActors(res);

	GL_CheckErrors("Frame: Post-SCR");

	//GL_CheckErrors("Frame");
	return res;
}

bool Application::InitializeController()
{
	ovrInputCapabilityHeader inputCapsHeader;
	for(uint32_t i = 0;
		vrapi_EnumerateInputDevices(mOvrMobile, i, &inputCapsHeader) == 0; ++i) {
		if(inputCapsHeader.Type == ovrControllerType_TrackedRemote) {
			mControllerID = inputCapsHeader.DeviceID;
			break;
		}
	}

	if((int)mControllerID != -1) {
		OVR_LOG("Found GearVR controller (ID: %x)", mControllerID);

		ovrInputTrackedRemoteCapabilities trackedInputCaps;
		trackedInputCaps.Header = inputCapsHeader;
		vrapi_GetInputDeviceCapabilities(mOvrMobile, &trackedInputCaps.Header);
		mTrackpadDim.x = trackedInputCaps.TrackpadMaxX;
		mTrackpadDim.y = trackedInputCaps.TrackpadMaxY;
		return true;
	}
	return false;
}

void Application::OnVideoStreamChanged(const avs::SetupCommand &setupCommand)
{
	if(mPipelineConfigured) {
		// TODO: Fix!
		return;
	}

	OVR_WARN("VIDEO STREAM CHANGED: %d %d %d", setupCommand.port, setupCommand.video_width, setupCommand.video_height);

	avs::NetworkSourceParams sourceParams = {};
	sourceParams.socketBufferSize = 64 * 1024 * 1024; // 64MiB socket buffer size
	//sourceParams.gcTTL = (1000/60) * 4; // TTL = 4 * expected frame time
	sourceParams.maxJitterBufferLength = 0;


	if(!mNetworkSource.configure(NumStreams + (GeoStream?1:0), setupCommand.port+1, mSession.GetServerIP().c_str(), setupCommand.port, sourceParams)) {
		OVR_WARN("OnVideoStreamChanged: Failed to configure network source node");
		return;
	}

	avs::DecoderParams decoderParams = {};
	decoderParams.codec = avs::VideoCodec::HEVC;
	decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
	decoderParams.prependStartCodes = false;
	decoderParams.deferDisplay = false;
	size_t stream_width=std::max(setupCommand.video_width,setupCommand.depth_width);
	size_t stream_height=setupCommand.video_height+setupCommand.depth_height;
	if(!mDecoder.configure(avs::DeviceHandle(), stream_width, stream_height, decoderParams, 50))
	{
		OVR_WARN("OnVideoStreamChanged: Failed to configure decoder node");
		mNetworkSource.deconfigure();
		return;
	}

	renderConstants.colourOffsetScale.x=0;
	renderConstants.colourOffsetScale.y = 0;
	renderConstants.colourOffsetScale.z = 1.0f;
	renderConstants.colourOffsetScale.w = float(setupCommand.video_height) / float(stream_height);

	renderConstants.depthOffsetScale.x = 0;
	renderConstants.depthOffsetScale.y = float(setupCommand.video_height) / float(stream_height);
	renderConstants.depthOffsetScale.z = float(setupCommand.depth_width) / float(stream_width);
	renderConstants.depthOffsetScale.w = float(setupCommand.depth_height) / float(stream_height);

	mSurface.configure(new VideoSurface(mVideoSurfaceTexture));

	mPipeline.link({&mNetworkSource, &mDecoder, &mSurface});

   //TODO: We will add a GEOMETRY PIPE:
   if(GeoStream)
   {
		avsGeometryDecoder.configure(100, &geometryDecoder);
		avsGeometryTarget.configure(&resourceCreator);
		mPipeline.link({ &mNetworkSource, &avsGeometryDecoder, &avsGeometryTarget });
   }

   mPipelineConfigured = true;
}

void Application::OnVideoStreamClosed()
{
	OVR_WARN("VIDEO STREAM CLOSED");

	mPipeline.deconfigure();
	mPipeline.reset();
	mPipelineConfigured = false;
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

void Application::RenderLocalActors(ovrFrameResult& res)
{
	scr::InputCommandCreateInfo ci;
	ci.type = scr::INPUT_COMMAND_MESH_MATERIAL_TRANSFORM;
	ci.pFBs = nullptr;
	ci.frameBufferCount = 0;
	ci.pCamera = nullptr;

	for(auto& actor : resourceManagers.mActorManager.m_Actors)
    {
        scr::InputCommand_Mesh_Material_Transform ic_mmm(&ci, actor.second.get());
        ic_mmm.pMaterial = mFlatColourMaterial.get();

        if(mOVRActors.find(actor.first) == mOVRActors.end())
        {
            const auto gl_effect = dynamic_cast<scc::GL_Effect*>(ic_mmm.pMaterial->GetMaterialCreateInfo().effect);
            const auto gl_effectPass = gl_effect->GetEffectPassCreateInfo("standard");
            const auto gl_vb = dynamic_cast<scc::GL_VertexBuffer*>(ic_mmm.pMesh->GetMeshCreateInfo().vb.get());
            const auto gl_ib = dynamic_cast<scc::GL_IndexBuffer*>(ic_mmm.pMesh->GetMeshCreateInfo().ib.get());
            gl_vb->CreateVAO(gl_ib->GetIndexID());

            GlGeometry geo;
            geo.vertexBuffer = gl_vb->GetVertexID();
            geo.indexBuffer = gl_ib->GetIndexID();
            geo.vertexArrayObject = gl_vb->GetVertexArrayID();
            geo.primitiveType = scc::GL_Effect::ToGLTopology(gl_effect->GetEffectPassCreateInfo("standard").topology);
            geo.vertexCount = (int) gl_vb->GetVertexCount();
            geo.indexCount = (int) gl_ib->GetIndexBufferCreateInfo().indexCount;
            GlGeometry::IndexType = gl_ib->GetIndexBufferCreateInfo().stride == 4 ? GL_UNSIGNED_INT : gl_ib->GetIndexBufferCreateInfo().stride == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

            ovrSurfaceDef ovr_Actor = {};
            std::string _actorName = std::string("ActorUID: ") + std::to_string(actor.first);
            ovr_Actor.surfaceName = _actorName;
            ovr_Actor.numInstances = 1;
            ovr_Actor.geo = geo;//BuildGlobe();

            ovr_Actor.graphicsCommand.Program = gl_effect->GetGlPlatform();

            ovr_Actor.graphicsCommand.GpuState.blendMode = scc::GL_Effect::ToGLBlendOp(gl_effectPass.colourBlendingState.colorBlendOp);
            ovr_Actor.graphicsCommand.GpuState.blendSrc = scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.srcColorBlendFactor);
            ovr_Actor.graphicsCommand.GpuState.blendDst = scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.dstColorBlendFactor);
            ovr_Actor.graphicsCommand.GpuState.blendModeAlpha = scc::GL_Effect::ToGLBlendOp(gl_effectPass.colourBlendingState.alphaBlendOp);
            ovr_Actor.graphicsCommand.GpuState.blendSrcAlpha = scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.srcAlphaBlendFactor);
            ovr_Actor.graphicsCommand.GpuState.blendDstAlpha = scc::GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.dstAlphaBlendFactor);
            ovr_Actor.graphicsCommand.GpuState.depthFunc = scc::GL_Effect::ToGLCompareOp(gl_effectPass.depthStencilingState.depthCompareOp);
            ovr_Actor.graphicsCommand.GpuState.frontFace = gl_effectPass.rasterizationState.frontFace == scr::Effect::FrontFace::COUNTER_CLOCKWISE ? GL_CCW : GL_CW;
            ovr_Actor.graphicsCommand.GpuState.polygonMode = scc::GL_Effect::ToGLPolygonMode(gl_effectPass.rasterizationState.polygonMode);
            ovr_Actor.graphicsCommand.GpuState.blendEnable = gl_effectPass.colourBlendingState.blendEnable ? OVR::ovrGpuState::ovrBlendEnable::BLEND_ENABLE : OVR::ovrGpuState::ovrBlendEnable::BLEND_DISABLE;
            ovr_Actor.graphicsCommand.GpuState.depthEnable = gl_effectPass.depthStencilingState.depthTestEnable;
            ovr_Actor.graphicsCommand.GpuState.depthMaskEnable = false;
            ovr_Actor.graphicsCommand.GpuState.colorMaskEnable[0] = true;
            ovr_Actor.graphicsCommand.GpuState.colorMaskEnable[1] = true;
            ovr_Actor.graphicsCommand.GpuState.colorMaskEnable[2] = true;
            ovr_Actor.graphicsCommand.GpuState.colorMaskEnable[3] = true;
            ovr_Actor.graphicsCommand.GpuState.polygonOffsetEnable = false;
            ovr_Actor.graphicsCommand.GpuState.cullEnable = true;
            ovr_Actor.graphicsCommand.GpuState.lineWidth = 1.0F;
            ovr_Actor.graphicsCommand.GpuState.depthRange[0] = gl_effectPass.depthStencilingState.minDepthBounds;
            ovr_Actor.graphicsCommand.GpuState.depthRange[1] = gl_effectPass.depthStencilingState.maxDepthBounds;

            mOVRActors[actor.first] = ovr_Actor;
        }

        float heightOffset = -0.80F;
        scr::mat4 inv_ue4ViewMatrix = scr::mat4::Translation(scr::vec3(-0.80F, -1.42F + heightOffset, 4.80F));
        scr::mat4 scr_Transform = inv_ue4ViewMatrix * ic_mmm.pTransform->GetTransformMatrix();

        OVR::Matrix4f transform;
        memcpy(&transform.M[0][0], &scr_Transform.a, 16 * sizeof(float));
        ovrDrawSurface ovr_ActorDrawSurface(transform, &mOVRActors[actor.first]);

        res.Surfaces.push_back(ovr_ActorDrawSurface);
    }
}
