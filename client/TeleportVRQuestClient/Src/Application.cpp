// (C) Copyright 2018-2021 Simul Software Ltd

#include "Application.h"

#include <sstream>

#include "GLESDebug.h"
#include "SimpleIni.h"

#include <libavstream/common.hpp>

#include "TeleportClient/ServerTimestamp.h"

#include "Config.h"
#include "Log.h"
#include "VideoSurface.h"

#include "OVRNodeManager.h"

#if defined( USE_AAUDIO )
#include "SCR_Class_AAudio_Impl/AA_AudioPlayer.h"
#else

#include "SCR_Class_SL_Impl/SL_AudioPlayer.h"

#endif

using namespace OVRFW;
using namespace teleport;
using namespace client;

extern "C"
{
	JNIEXPORT jlong Java_co_simul_teleportvrquestclient_MainActivity_nativeInitFromJava(JNIEnv *jni)
	{
		VideoDecoderProxy::InitializeJNI(jni);
		Android_MemoryUtil::InitializeJNI(jni);

		return 0;
	}
} // extern "C"

Application::Application()
		: ovrAppl(0, 0, CPU_LEVEL, GPU_LEVEL, true /* useMultiView */)
		  , Locale(nullptr)
		  , Random(2)
		  , mSoundEffectPlayer(nullptr)
		  , mGuiSys(nullptr)
			,clientRenderer(this, &clientDeviceState,&controllers,this)
		  , lobbyRenderer(&clientDeviceState, this)
{
	RedirectStdCoutCerr();

	pthread_setname_np(pthread_self(), "Teleport_Application");
	mContext.setMessageHandler(Application::avsMessageHandler, this);

	if (enet_initialize() != 0)
	{
		OVR_FAIL("Failed to initialize ENET library");
	}

	if (clientRenderer.AudioStream)
	{
#if defined( USE_AAUDIO )
		audioPlayer = new AA_AudioPlayer();
#else
		clientRenderer.audioPlayer = new SL_AudioPlayer();
#endif
		clientRenderer.audioPlayer ->initializeAudioDevice();
	}
}

Application::~Application()
{
	clientRenderer.clientPipeline.pipeline.deconfigure();
	mRefreshRates.clear();
	clientRenderer.ExitedVR();
	delete mSoundEffectPlayer;
	enet_deinitialize();
	SAFE_DELETE(clientRenderer.audioPlayer)

	mSoundEffectPlayer = nullptr;

	OvrGuiSys::Destroy(mGuiSys);
}

// Returns a random float in the range [0, 1].
float Application::RandomFloat()
{
	Random = 1664525L * Random + 1013904223L;
	unsigned int rf = 0x3F800000 | (Random & 0x007FFFFF);
	return (*(float *) &rf) - 1.0f;
}

bool Application::ProcessIniFile()
{
	std::string client_ini = LoadTextFile("client.ini");
	CSimpleIniA ini;

	SI_Error rc = ini.LoadData(client_ini.data(), client_ini.length());
	if (rc == SI_OK)
	{
		std::vector<std::string> server_ips;
		std::string ip_list;
		ip_list = ini.GetValue("", "SERVER_IP", "");

		size_t pos = 0;
		std::string token;
		do
		{
			pos = ip_list.find(",");
			std::string ip=ip_list.substr(0, pos);
			server_ips.push_back(ip);
			ip_list.erase(0, pos + 1);
			tinyUI.AddURL(ip);
		} while (pos != std::string::npos);
		clientRenderer.server_discovery_port = ini.GetLongValue("", "SERVER_DISCOVERY_PORT",TELEPORT_SERVER_DISCOVERY_PORT);
		clientRenderer.client_service_port = ini.GetLongValue("", "CLIENT_SERVICE_PORT",TELEPORT_CLIENT_SERVICE_PORT);
		clientRenderer.client_streaming_port = ini.GetLongValue("", "CLIENT_STREAMING_PORT",TELEPORT_CLIENT_STREAMING_PORT);

		return true;
	}
	else
	{
		std::cerr << "Create client.ini in assets directory to specify settings." << std::endl;
		return false;
	}
}


static void SetObjectText(OvrGuiSys& guiSys, VRMenu* menu, char const* name, char const* fmt, ...)
{
	VRMenuObject* obj = menu->ObjectForName(guiSys, name);
	if (obj != nullptr) {
		char text[1024];
		va_list argPtr;
		va_start(argPtr, fmt);
		OVR::OVR_vsprintf(text, sizeof(text), fmt, argPtr);
		va_end(argPtr);
		obj->SetText(text);
	}
}

bool Application::AppInit(const OVRFW::ovrAppContext *context)
{
	RedirectStdCoutCerr();
	const ovrJava &jj = *(reinterpret_cast<const ovrJava *>(context->ContextForVrApi()));
	const xrJava ctx = JavaContextConvert(jj);
	//const ovrInitParms initParms = vrapi_DefaultInitParms(&jj);
	FileSys = OVRFW::ovrFileSys::Create(ctx);
	if (nullptr == FileSys)
	{
		ALOGE("Couldn't create FileSys");
		return false;
	}

	Locale = ovrLocale::Create(*ctx.Env, ctx.ActivityObject, "default");
	if (nullptr == Locale)
	{
		ALOGE("Couldn't create Locale");
		return false;
	}

	mSoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();
	if (nullptr == mSoundEffectPlayer)
	{
		ALOGE("Couldn't create mSoundEffectPlayer");
		return false;
	}

	mGuiSys = OvrGuiSys::Create(&ctx);
	if (nullptr == mGuiSys)
	{
		ALOGE("Couldn't create GUI");
		return false;
	}

	std::string fontName;
	Locale->GetLocalizedString("@string/font_name", "efigs.fnt", fontName);
	mGuiSys->Init(FileSys, *mSoundEffectPlayer, fontName.c_str(), nullptr);

	ProcessIniFile();

	if (false == tinyUI.Init(&ctx, FileSys,mGuiSys,Locale))
	{
		ALOG("TinyUI::Init FAILED.");
	}
	auto connectButtonHandler = std::bind(&Application::ConnectButtonHandler, this,
			std::placeholders::_1);
	tinyUI.SetConnectHandler(connectButtonHandler);
	//menu = ovrControllerGUI::Create(this,mGuiSys,Locale);
	if (menu != nullptr)
	{
		mGuiSys->AddMenu(menu);
		mGuiSys->OpenMenu(menu->GetName());

		OVR::Posef pose = menu->GetMenuPose();
		pose.Translation = Vector3f(0.0f, 1.0f, -2.0f);
		menu->SetMenuPose(pose);

		SetObjectText(*mGuiSys, menu, "panel", "VrInput");
	}

	SurfaceRender.Init();

	startTime = GetTimeInSeconds();

	EnteredVrMode();
	return true;
}

void Application::EnteredVrMode()
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	mDeviceContext.reset(new scc::GL_DeviceContext(&globalGraphicsResources.renderPlatform));

	//Default Effects
	clientrender::Effect::EffectCreateInfo ci;
	ci.effectName = "StandardEffects";
	globalGraphicsResources.defaultPBREffect.Create(&ci);

	//Default Sampler
	clientrender::Sampler::SamplerCreateInfo sci = {};
	sci.wrapU = clientrender::Sampler::Wrap::REPEAT;
	sci.wrapV = clientrender::Sampler::Wrap::REPEAT;
	sci.wrapW = clientrender::Sampler::Wrap::REPEAT;
	sci.minFilter = clientrender::Sampler::Filter::LINEAR;
	sci.magFilter = clientrender::Sampler::Filter::LINEAR;

	globalGraphicsResources.sampler = globalGraphicsResources.renderPlatform.InstantiateSampler();
	globalGraphicsResources.sampler->Create(&sci);

	globalGraphicsResources.noMipsampler = globalGraphicsResources.renderPlatform.InstantiateSampler();
	globalGraphicsResources.noMipsampler->Create(&sci);

	sci.minFilter = clientrender::Sampler::Filter::MIPMAP_LINEAR;
	globalGraphicsResources.cubeMipMapSampler = globalGraphicsResources.renderPlatform.InstantiateSampler();
	globalGraphicsResources.cubeMipMapSampler->Create(&sci);

	OVR_LOG("%s | %s", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &globalGraphicsResources.maxFragTextureSlots);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &globalGraphicsResources.maxFragUniformBlocks);
	OVR_LOG("Fragment Texture Slots: %d, Fragment Uniform Blocks: %d",
			globalGraphicsResources.maxFragTextureSlots,
			globalGraphicsResources.maxFragUniformBlocks);

	//Setup Debug
	scc::SetupGLESDebug();

	/// Get JNI
	const ovrJava *java = (reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
	TempJniEnv env(java->Vm);
	mSoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

	//mLocale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");
	//std::string fontName;
	//GetLocale().GetString("@string/font_name", "efigs.fnt", fontName);

	memoryUtil.reset(new Android_MemoryUtil(java->Env));

	clientRenderer.EnteredVR(java);

	clientRenderer.clientPipeline.decoder.setBackend(new VideoDecoderProxy(java->Env, this));


	//Set Lighting Cubemap Shader Resource
	clientrender::ShaderResourceLayout lightingCubemapLayout;
	lightingCubemapLayout.AddBinding(14,
									 clientrender::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,
									 clientrender::Shader::Stage::SHADER_STAGE_FRAGMENT);
	lightingCubemapLayout.AddBinding(15,
									 clientrender::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,
									 clientrender::Shader::Stage::SHADER_STAGE_FRAGMENT);

	globalGraphicsResources.lightCubemapShaderResources.SetLayout(lightingCubemapLayout);
	globalGraphicsResources.lightCubemapShaderResources.AddImage(
			clientrender::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 14,
			"u_SpecularCubemap", {clientRenderer.specularCubemapTexture->GetSampler()
								  , clientRenderer.specularCubemapTexture});
	globalGraphicsResources.lightCubemapShaderResources.AddImage(
			clientrender::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 15,
			"u_DiffuseCubemap", {clientRenderer.diffuseCubemapTexture->GetSampler()
								 , clientRenderer.diffuseCubemapTexture});

	clientrender::ShaderResourceLayout tagBufferLayout;
	tagBufferLayout.AddBinding(1,
									 clientrender::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER,
									 clientrender::Shader::Stage::SHADER_STAGE_FRAGMENT);
	globalGraphicsResources.tagShaderResource.SetLayout(tagBufferLayout);
	globalGraphicsResources.tagShaderResource.AddBuffer(clientrender::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER,1,"TagDataCube_ssbo",{
			globalGraphicsResources.mTagDataBuffer.get()});
	globalGraphicsResources.Init();
	//useMultiview=(vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_MULTIVIEW_AVAILABLE ) == VRAPI_TRUE);
	int num_refresh_rates = vrapi_GetSystemPropertyInt(java,
													   VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
	mRefreshRates.resize(num_refresh_rates);
	vrapi_GetSystemPropertyFloatArray(java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,
									  mRefreshRates.data(), num_refresh_rates);


	DeviceType = (ovrDeviceType)vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_DEVICE_TYPE);

	int32_t minPriority=0;
	if((int)DeviceType>=(int)VRAPI_DEVICE_TYPE_OCULUSQUEST2_START)
		minPriority=-10000;
	clientRenderer.SetMinimumPriority(minPriority);
	//if (num_refresh_rates > 0)
	//	vrapi_SetDisplayRefreshRate(app->GetOvrMobile(), mRefreshRates[num_refresh_rates - 1]);

	// Bind the delegates.

	controllers.SetCycleShaderModeDelegate(std::bind(&ClientRenderer::CycleShaderMode, &clientRenderer));
	controllers.SetCycleOSDDelegate(std::bind(&ClientRenderer::CycleOSD, &clientRenderer));
	controllers.SetCycleOSDSelectionDelegate(std::bind(&ClientRenderer::CycleOSDSelection, &clientRenderer));
	controllers.SetToggleMenuDelegate(std::bind(&Application::ToggleMenu, this));
	controllers.SetDebugOutputDelegate(std::bind(&ClientRenderer::WriteDebugOutput, &clientRenderer));
	controllers.SetToggleWebcamDelegate(std::bind(&ClientRenderer::ToggleWebcam, &clientRenderer));
	controllers.SetSetStickOffsetDelegate(std::bind(&ClientRenderer::SetStickOffset, &clientRenderer, std::placeholders::_1, std::placeholders::_2));

	//uIRenderer.Init();
}

void Application::AppShutdown(const OVRFW::ovrAppContext *)
{
	ALOGV("AppShutdown - enter");
	tinyUI.Shutdown();
	SurfaceRender.Shutdown();
	OVRFW::ovrFileSys::Destroy(FileSys);
	RenderState = RENDER_STATE_ENDING;
	ALOGV("AppShutdown - exit");
}

void Application::AppResumed(const OVRFW::ovrAppContext * /* context */)
{
	ALOGV("ovrSampleAppl::AppResumed");
	RenderState = RENDER_STATE_RUNNING;
}

void Application::AppPaused(const OVRFW::ovrAppContext * /* context */)
{
	ALOGV("ovrSampleAppl::AppPaused");
}

OVRFW::ovrApplFrameOut Application::AppFrame(const OVRFW::ovrApplFrameIn &vrFrame)
{
	// process input events first because this mirrors the behavior when OnKeyEvent was
	// a virtual function on VrAppInterface and was called by VrAppFramework.
	for (int i = 0; i < static_cast<int>(vrFrame.KeyEvents.size()); i++)
	{
		const int keyCode = vrFrame.KeyEvents[i].KeyCode;
		const int action = vrFrame.KeyEvents[i].Action;

		if (mGuiSys->OnKeyEvent(keyCode, action))
		{
			continue;
		}
	}

	//CenterEyeViewMatrix = OVR::Matrix4f(vrFrame.HeadPose);
	OVRFW::ovrApplFrameOut out=Frame(vrFrame);
	return out;
}

OVRFW::ovrApplFrameOut Application::Frame(const OVRFW::ovrApplFrameIn& vrFrame)
{
	if(!VideoDecoderProxy::IsJNIInitialized())
		return OVRFW::ovrApplFrameOut();
	// we don't want local slide movements.
	mScene.SetMoveSpeed(1.0f);

	// Try to find remote controller
	if(controllers.mControllerIDs[0] == 0)
	{
		controllers.InitializeController(GetSessionObject(),0);
	}
	if(controllers.mControllerIDs[1] == 0)
	{
		controllers.InitializeController(GetSessionObject(),1);
	}
	controllers.Update(GetSessionObject());

	clientDeviceState.originPose.position=*((const avs::vec3*)&mScene.GetFootPos());
	clientDeviceState.eyeHeight=mScene.GetEyeHeight();

	// Oculus Origin means where the headset's zero is in real space.
	auto &sessionClient=clientRenderer.sessionClient;
	// Handle networked session.
	if(sessionClient.IsConnected())
	{
		avs::DisplayInfo displayInfo = {1440, 1600};
		avs::Pose controllerPoses[2];
		controllerPoses[0]=clientDeviceState.controllerPoses[0].globalPose;
		controllerPoses[1]=clientDeviceState.controllerPoses[1].globalPose;
		sessionClient.Frame(displayInfo, clientDeviceState.headPose.globalPose, controllerPoses, receivedInitialPos, clientDeviceState.originPose, controllers.mLastControllerStates, clientRenderer.clientPipeline.decoder.idrRequired(), vrFrame.RealTimeInSeconds, vrFrame.DeltaSeconds);
		if (sessionClient.receivedInitialPos>0&&receivedInitialPos!=sessionClient.receivedInitialPos)
		{
			clientDeviceState.originPose = sessionClient.GetOriginPose();
			mScene.SetFootPos(*((const OVR::Vector3f*)&clientDeviceState.originPose.position));
			float yaw_angle=2.0f*atan2(clientDeviceState.originPose.orientation.y,clientDeviceState.originPose.orientation.w);
			mScene.SetStickYaw(yaw_angle);
			receivedInitialPos = sessionClient.receivedInitialPos;
			if(receivedRelativePos!=sessionClient.receivedRelativePos)
			{
				receivedRelativePos=sessionClient.receivedRelativePos;
				//avs::vec3 pos =sessionClient.GetOriginToHeadOffset();
				//camera.SetPosition((const float*)(&pos));
			}
		}
	}
	else if(should_connect)
	{
		if (!sessionClient.HasDiscovered())
		{
			sessionClient.Discover("", TELEPORT_CLIENT_DISCOVERY_PORT, clientRenderer.server_ip.c_str(), clientRenderer.server_discovery_port, remoteEndpoint);
		}
		if (sessionClient.HasDiscovered())
		{
			// if connect fails, restart discovery.
			if(!sessionClient.Connect(remoteEndpoint, TELEPORT_TIMEOUT))
				sessionClient.Disconnect(0);
			else
				tinyUI.HideMenu();
			should_connect=false;
		}
	}
	mScene.Frame(vrFrame,-1,false);
	clientRenderer.eyeSeparation=vrFrame.IPD;
	GLCheckErrorsWithTitle("Frame: Start");

	//Get HMD Position/Orientation
	clientDeviceState.stickYaw=mScene.GetStickYaw();
	clientDeviceState.SetHeadPose(*((const avs::vec3 *)(&vrFrame.HeadPose.Translation)),*((const clientrender::quat *)(&vrFrame.HeadPose.Rotation)));
	clientDeviceState.UpdateGlobalPoses();
	// Update video texture if we have any pending decoded frames.

    if (mNumPendingFrames > 0)
    {
        clientRenderer.mVideoSurfaceTexture->Update();
        if (clientRenderer.clientPipeline.videoConfig.use_alpha_layer_decoding)
        {
            clientRenderer.mAlphaSurfaceTexture->Update();
        }
        mNumPendingFrames = 0;
    }

	// Process stream pipeline
	clientRenderer.clientPipeline.pipeline.process();

    std::vector<TinyUI::ControllerState> states;
    states.push_back({});
	states.push_back({});
	states[0].pose.Translation	=*((const OVR::Vector3f*)&clientDeviceState.controllerPoses[0].globalPose.position);
	states[0].pose.Rotation		=*((const OVR::Quatf *)&clientDeviceState.controllerPoses[0].globalPose.orientation);
	states[0].clicking			=(controllers.mLastControllerStates[0].mReleased& ovrButton::ovrButton_Trigger)!=0;
	states[1].pose.Translation	=*((const OVR::Vector3f*)&clientDeviceState.controllerPoses[1].globalPose.position);
	states[1].pose.Rotation		=*((const OVR::Quatf *)&clientDeviceState.controllerPoses[1].globalPose.orientation);
	states[1].clicking			=(controllers.mLastControllerStates[1].mReleased& ovrButton::ovrButton_Trigger)!=0;
	/// Hit test
	tinyUI.DoHitTests(vrFrame,states);
    tinyUI.Update(vrFrame);
	return OVRFW::ovrApplFrameOut();
}

void Application::ConnectButtonHandler(const std::string &url)
{
	auto &sessionClient=clientRenderer.sessionClient;
	if(sessionClient.IsConnected())
	{
		sessionClient.Disconnect(0);
	}
	size_t pos=url.find(":");
	if(pos<url.length())
	{
		std::string port_str=url.substr(pos+1,url.length()-pos-1);
		clientRenderer.server_discovery_port = atoi(port_str.c_str());
		std::string url_str=url.substr(0,pos);
		clientRenderer.server_ip=url_str;
	}
	else
	{
		clientRenderer.server_ip=url;
	}
	should_connect=true;
}

void Application::AppRenderFrame(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out)
{
	switch (RenderState)
	{
		case RENDER_STATE_LOADING:
		{
			DefaultRenderFrame_Loading(in, out);
		}
			break;
		case RENDER_STATE_RUNNING:
		{
			/// Frame matrices
			for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
			{
				out.FrameMatrices.EyeView[eye] = in.Eye[eye].ViewMatrix;
				// Calculate projection matrix using custom near plane value.
				out.FrameMatrices.EyeProjection[eye] = ovrMatrix4f_CreateProjectionFov(
						SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, 0.0f, 0.0f, 0.1f,
						7.0f);
			}

			Render(in, out);
			DefaultRenderFrame_Running(in, out);
		}
			break;
		case RENDER_STATE_ENDING:
		{
			DefaultRenderFrame_Ending(in, out);
		}
			break;
	}
}

void Application::DrawTexture(avs::vec3 &offset,clientrender::Texture &texture)
{
	//mGuiSys->ShowInfoText()
}

void Application::PrintText(avs::vec3 &offset,avs::vec4 &colour,const char *txt,...)
{
	static char txt2[2000];
	va_list args;
	va_start(args, txt);
	vsprintf(txt2,txt,args);
	va_end(args);
	OVRFW::BitmapFontSurface &fontSurface=mGuiSys->GetDefaultFontSurface();
	auto GetViewMatrixPosition = [](Matrix4f const& m)
			{
				return m.Inverted().GetTranslation();
			};
	auto GetViewMatrixForward= [](Matrix4f const& m)
			{
				return Vector3f(-m.M[2][0], -m.M[2][1], -m.M[2][2]);
			};

	auto GetViewMatrixUp= [](Matrix4f const& m)
			{
				return Vector3f(m.M[1][0], m.M[1][1], m.M[1][2]);
			};
	Vector3f viewPos	=GetViewMatrixPosition(lastCenterView);
	Vector3f viewFwd	=GetViewMatrixForward(lastCenterView);
	Vector3f viewUp		=GetViewMatrixUp(lastCenterView);
	Vector3f viewRight = viewFwd.Cross(viewUp);
	Vector3f newPos		= viewPos + viewFwd * offset.z + viewUp * offset.y +viewRight * offset.x;
	fontParms_t fp;
	fp.AlignHoriz = HORIZONTAL_CENTER;
	fp.AlignVert = VERTICAL_CENTER;
	fp.Billboard = true;
	fp.TrackRoll = false;
	fontSurface.DrawTextBillboarded3D(
			mGuiSys->GetDefaultFont(),
			fp,
			newPos,
			1.0f,
			*((OVR::Vector4f*)&colour),
			txt2);
}

void Application::DrawConnectionStateOSD(OVRFW::OvrGuiSys *mGuiSys,OVRFW::ovrRendererOutput &out)
{
	static avs::vec3 offset = {-2.0f, 2.0f, 5.5f};
	static avs::vec4 colour = {1.0f, 1.0f, 0.5f, 0.5f};
	static char txt[100];
	auto &sessionClient=clientRenderer.sessionClient;
	if (sessionClient.IsConnected())
	{
		sprintf(txt,"Server: %s", sessionClient.GetServerIP().c_str());
	}
	else
	{
		sprintf(txt,"Unconnected");
	}
	PrintText(offset,colour,txt);
	static avs::vec3 offset2 = {2.0f, 2.0f, 6.0f};
	PrintText(offset2,colour,"%4.1f fps",frameRate);
}

void Application::Render(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out)
{
	if(!VideoDecoderProxy::IsJNIInitialized())
		return ;
	if (in.DeltaSeconds > 0.0f)
	{
		frameRate *= 0.99f;
		frameRate += 0.01f / in.DeltaSeconds;
	}
//Build frame
	mScene.GetFrameMatrices(SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
	lastCenterView=out.FrameMatrices.CenterView;
	mScene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
// The camera should be where our head is. But when rendering, the camera is in OVR space, so:
	globalGraphicsResources.scrCamera->UpdatePosition(clientDeviceState.headPose.globalPose.position);

	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;

	GLCheckErrorsWithTitle("Frame: Pre-Cubemap");
	clientRenderer.CopyToCubemaps(*mDeviceContext);

    //Append SCR Nodes to surfaces.
	GLCheckErrorsWithTitle("Frame: Pre-SCR");
	float timestamp_ms = GetTimeInSeconds() * 1000.0f;
	// TODO: is this the timestamp we want, or do we need something server-synchronized?
	clientRenderer.Update(timestamp_ms);
    //Move the hands before they are drawn.
	clientRenderer.UpdateHandObjects(GetSessionObject());
    clientRenderer.RenderLocalNodes(out);
	auto &sessionClient=clientRenderer.sessionClient;
	if (sessionClient.IsConnected())
	{
		// Append video surface
		clientRenderer.RenderVideo(*mDeviceContext, out);
		clientRenderer.RenderWebcam(out);
	}
	else
	{
		lobbyRenderer.Render(clientRenderer.server_ip.c_str(),clientRenderer.server_discovery_port);
	};
	//uIRenderer.Render(in,out,1440,1600);
	tinyUI.Render(in, out);
	clientRenderer.DrawOSD(out);
	DrawConnectionStateOSD(mGuiSys, out);
	GLCheckErrorsWithTitle("Frame: Post-SCR");
}

void Application::AppRenderEye(const OVRFW::ovrApplFrameIn &vrFrame, OVRFW::ovrRendererOutput &out, int eye)
{
	// Update GUI systems last, but before rendering anything.
	mGuiSys->Frame(vrFrame, out.FrameMatrices.CenterView);
// Append GuiSys surfaces. This should always be the last item to append the render list.
	mGuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);
	// Render the surfaces returned by Frame.
	SurfaceRender.RenderSurfaceList(
			out.Surfaces,
			out.FrameMatrices.EyeView[0], // always use 0 as it assumes an array
			out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
			eye);
}

bool Application::OnSetupCommandReceived(const char *server_ip, const avs::SetupCommand &setupCommand, avs::Handshake &handshake)
{
	return clientRenderer.OnSetupCommandReceived(server_ip,setupCommand,handshake);
}

void Application::OnLightingSetupChanged(const avs::SetupLightingCommand &s)
{
	clientRenderer.lastSetupLightingCommand=s;
}

void Application::UpdateNodeStructure(const avs::UpdateNodeStructureCommand &updateNodeStructureCommand)
{
	clientRenderer.geometryCache.mNodeManager->ReparentNode(updateNodeStructureCommand);
}

void Application::UpdateNodeSubtype(const avs::UpdateNodeSubtypeCommand &updateNodeSubTypeCommand)
{
	switch(updateNodeSubTypeCommand.nodeSubtype)
	{
		case avs::NodeSubtype::None:
			break;
		case avs::NodeSubtype::Body:
			clientRenderer.geometryCache.mNodeManager->SetBody(updateNodeSubTypeCommand.nodeID);
			break;
		case avs::NodeSubtype::LeftHand:
			clientRenderer.geometryCache.mNodeManager->SetLeftHand(updateNodeSubTypeCommand.nodeID);
			break;
		case avs::NodeSubtype::RightHand:
			clientRenderer.geometryCache.mNodeManager->SetRightHand(updateNodeSubTypeCommand.nodeID);
			break;
		default:
			TELEPORT_CERR << "Unrecognised node data sub-type: " << static_cast<int>(updateNodeSubTypeCommand.nodeSubtype) << "!\n";
			break;
	}
}
void Application::OnVideoStreamClosed()
{
	OVR_WARN("VIDEO STREAM CLOSED");

	clientRenderer.clientPipeline.pipeline.deconfigure();
	clientRenderer.clientPipeline.pipeline.reset();
	clientRenderer.mPipelineConfigured = false;

	receivedInitialPos = false;
}

void Application::OnReconfigureVideo(const avs::ReconfigureVideoCommand &reconfigureVideoCommand)
{
	if (!clientRenderer.mPipelineConfigured)
	{
		return;
	}

	clientRenderer.ConfigureVideo(reconfigureVideoCommand.video_config);
	TELEPORT_CLIENT_WARN("VIDEO STREAM RECONFIGURED: clr %d x %d dpth %d x %d",
		 clientRenderer.clientPipeline.videoConfig.video_width, clientRenderer.clientPipeline.videoConfig.video_height,
		 clientRenderer.clientPipeline.videoConfig.depth_width, clientRenderer.clientPipeline.videoConfig.depth_height);
}

bool Application::OnNodeEnteredBounds(avs::uid id)
{
	return clientRenderer.geometryCache.mNodeManager->ShowNode(id);
}

bool Application::OnNodeLeftBounds(avs::uid id)
{
	return clientRenderer.geometryCache.mNodeManager->HideNode(id);
}

std::vector<uid> Application::GetGeometryResources()
{
	return clientRenderer.geometryCache.GetAllResourceIDs();
}

void Application::ClearGeometryResources()
{
	clientRenderer.geometryCache.Clear();
}

void Application::SetVisibleNodes(const std::vector<avs::uid> &visibleNodes)
{
	clientRenderer.geometryCache.mNodeManager->SetVisibleNodes(visibleNodes);
}

void Application::UpdateNodeMovement(const std::vector<avs::MovementUpdate> &updateList)
{
	clientRenderer.geometryCache.mNodeManager->UpdateNodeMovement(updateList);
}

void Application::UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList)
{
	clientRenderer.geometryCache.mNodeManager->UpdateNodeEnabledState(updateList);
}

void Application::SetNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	clientRenderer.geometryCache.mNodeManager->SetNodeHighlighted(nodeID, isHighlighted);
}

void Application::UpdateNodeAnimation(const avs::ApplyAnimation& animationUpdate)
{
	clientRenderer.geometryCache.mNodeManager->UpdateNodeAnimation(animationUpdate);
}

void Application::UpdateNodeAnimationControl(const avs::NodeUpdateAnimationControl& animationControlUpdate)
{
	switch(animationControlUpdate.timeControl)
	{
		case avs::AnimationTimeControl::ANIMATION_TIME:
			clientRenderer.geometryCache.mNodeManager->UpdateNodeAnimationControl(animationControlUpdate.nodeID, animationControlUpdate.animationID);
			break;
		case avs::AnimationTimeControl::CONTROLLER_0_TRIGGER:
			clientRenderer.geometryCache.mNodeManager->UpdateNodeAnimationControl(animationControlUpdate.nodeID, animationControlUpdate.animationID, &controllers.mLastControllerStates[0].triggerBack, 1.0f);
			break;
		case avs::AnimationTimeControl::CONTROLLER_1_TRIGGER:
			clientRenderer.geometryCache.mNodeManager->UpdateNodeAnimationControl(animationControlUpdate.nodeID, animationControlUpdate.animationID, &controllers.mLastControllerStates[1].triggerBack, 1.0f);
			break;
		default:
			TELEPORT_CLIENT_WARN("Failed to update node animation control! Time control was set to the invalid value %d!", static_cast<int>(animationControlUpdate.timeControl));
			break;
	}
}

void Application::SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed)
{
	clientRenderer.geometryCache.mNodeManager->SetNodeAnimationSpeed(nodeID, animationID, speed);
}

void Application::OnFrameAvailable()
{
	++mNumPendingFrames;
}

void Application::avsMessageHandler(avs::LogSeverity severity, const char *msg, void *)
{
	switch (severity)
	{
		case avs::LogSeverity::Error:
		case avs::LogSeverity::Warning:
			if (msg)
			{
				static std::ostringstream ostr;
				while ((*msg) != 0 && (*msg) != '\n')
				{
					ostr << (*msg);
					msg++;
				}
				if (*msg == '\n')
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
			if (msg)
			{
				static std::ostringstream ostr;
				while ((*msg) != 0 && (*msg) != '\n')
				{
					ostr << (*msg);
					msg++;
				}
				if (*msg == '\n')
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

const clientrender::Effect::EffectPassCreateInfo *Application::BuildEffectPass(const char *effectPassName, clientrender::VertexBufferLayout *vbl
							 , const clientrender::ShaderSystem::PipelineCreateInfo *pipelineCreateInfo
							 , const std::vector<clientrender::ShaderResource> &shaderResources)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	if (globalGraphicsResources.defaultPBREffect.HasEffectPass(effectPassName))
	{
		return globalGraphicsResources.defaultPBREffect.GetEffectPassCreateInfo(effectPassName);
	}

	clientrender::ShaderSystem::PassVariables pv;
	pv.mask = false;
	pv.reverseDepth = false;
	pv.msaa = false;

	clientrender::ShaderSystem::Pipeline gp(&globalGraphicsResources.renderPlatform, pipelineCreateInfo);

	//clientrender::VertexBufferLayout
	vbl->CalculateStride();

	clientrender::Effect::ViewportAndScissor vs = {};
	vs.x = 0.0f;
	vs.y = 0.0f;
	vs.width = 0.0f;
	vs.height = 0.0f;
	vs.minDepth = 1.0f;
	vs.maxDepth = 0.0f;
	vs.offsetX = 0;
	vs.offsetY = 0;
	vs.extentX = (uint32_t) vs.x;
	vs.extentY = (uint32_t) vs.y;

	clientrender::Effect::RasterizationState rs = {};
	rs.depthClampEnable = false;
	rs.rasterizerDiscardEnable = false;
	rs.polygonMode = clientrender::Effect::PolygonMode::FILL;
	rs.cullMode = clientrender::Effect::CullMode::FRONT_BIT; //As of 2020-02-24, this only affects whether culling is enabled.
	rs.frontFace = clientrender::Effect::FrontFace::COUNTER_CLOCKWISE; //Unity does clockwise winding, and Unreal does counter-clockwise, but this is set before we connect to a server.

	clientrender::Effect::MultisamplingState ms = {};
	ms.samplerShadingEnable = false;
	ms.rasterizationSamples = clientrender::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT;

	clientrender::Effect::StencilCompareOpState scos = {};
	scos.stencilFailOp = clientrender::Effect::StencilCompareOp::KEEP;
	scos.stencilPassDepthFailOp = clientrender::Effect::StencilCompareOp::KEEP;
	scos.passOp = clientrender::Effect::StencilCompareOp::KEEP;
	scos.compareOp = clientrender::Effect::CompareOp::NEVER;
	clientrender::Effect::DepthStencilingState dss = {};
	dss.depthTestEnable = true;
	dss.depthWriteEnable = true;
	dss.depthCompareOp = clientrender::Effect::CompareOp::LESS;
	dss.stencilTestEnable = false;
	dss.frontCompareOp = scos;
	dss.backCompareOp = scos;
	dss.depthBoundTestEnable = false;
	dss.minDepthBounds = 0.0f;
	dss.maxDepthBounds = 1.0f;

	clientrender::Effect::ColourBlendingState cbs = {};
	cbs.blendEnable = true;
	cbs.srcColorBlendFactor = clientrender::Effect::BlendFactor::SRC_ALPHA;
	cbs.dstColorBlendFactor = clientrender::Effect::BlendFactor::ONE_MINUS_SRC_ALPHA;
	cbs.colorBlendOp = clientrender::Effect::BlendOp::ADD;
	cbs.srcAlphaBlendFactor = clientrender::Effect::BlendFactor::ONE;
	cbs.dstAlphaBlendFactor = clientrender::Effect::BlendFactor::ZERO;
	cbs.alphaBlendOp = clientrender::Effect::BlendOp::ADD;

	clientrender::Effect::EffectPassCreateInfo ci;
	ci.effectPassName = effectPassName;
	ci.passVariables = pv;
	ci.pipeline = gp;
	ci.vertexLayout = *vbl;
	ci.topology = clientrender::Effect::TopologyType::TRIANGLE_LIST;
	ci.viewportAndScissor = vs;
	ci.rasterizationState = rs;
	ci.multisamplingState = ms;
	ci.depthStencilingState = dss;
	ci.colourBlendingState = cbs;

	globalGraphicsResources.defaultPBREffect.CreatePass(&ci);
	globalGraphicsResources.defaultPBREffect.LinkShaders(effectPassName, shaderResources);

	return globalGraphicsResources.defaultPBREffect.GetEffectPassCreateInfo(effectPassName);
}

std::string Application::LoadTextFile(const char *filename)
{
	std::vector<uint8_t> outBuffer;
	std::string str = "apk:///assets/";
	str += filename;
	if (mGuiSys && mGuiSys->GetFileSys().ReadFile(str.c_str(), outBuffer))
	{
		if (outBuffer.back() != '\0')
		{
			outBuffer.push_back('\0');
		} //Append Null terminator character. ReadFile() does not return a null terminated string, apparently!
		return std::string((const char *)outBuffer.data());
	}
	return "";
}
void Application::ToggleMenu()
{
	ovrVector3f pos;
	pos.x=clientDeviceState.headPose.globalPose.position.x;
	pos.y=clientDeviceState.headPose.globalPose.position.y;
	pos.z=clientDeviceState.headPose.globalPose.position.z;
	ovrVector4f orient;
	orient.x=clientDeviceState.headPose.globalPose.orientation.x;
	orient.y=clientDeviceState.headPose.globalPose.orientation.y;
	orient.z=clientDeviceState.headPose.globalPose.orientation.z;
	orient.w=clientDeviceState.headPose.globalPose.orientation.w;
	tinyUI.ToggleMenu(pos,orient);
}



//==============================================================
// android_main
//==============================================================
void android_main(struct android_app *app)
{
	std::unique_ptr<OVRFW::Application> appl =
			std::unique_ptr<OVRFW::Application>(new OVRFW::Application());
	appl->Run(app);
}
