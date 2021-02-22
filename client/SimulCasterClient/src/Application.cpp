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
#include "Log.h"
#include "OVRNodeManager.h"
#include "VideoSurface.h"
#include <libavstream/common.hpp>

#include "SimpleIni.h"

#if defined( OVR_OS_WIN32 )
#include "../res_pc/resource.h"
#endif

#if defined( USE_AAUDIO )
#include "SCR_Class_AAudio_Impl/AA_AudioPlayer.h"
#else
#include "SCR_Class_SL_Impl/SL_AudioPlayer.h"
#endif

using namespace OVR;


#if defined( OVR_OS_ANDROID )
extern "C"
{
	jlong Java_co_Simul_remoteplayclient_MainActivity_nativeSetAppInterface(JNIEnv* jni, jclass clazz, jobject activity,
			jstring fromPackageName, jstring commandString, jstring uriString )
	{
		VideoDecoderProxy::InitializeJNI(jni);
		return (new Application())->SetActivity(jni, clazz, activity, fromPackageName, commandString, uriString);
	}
} // extern "C"

#endif

ovrQuatf QuaternionMultiply(const ovrQuatf &p,const ovrQuatf &q)
{
	ovrQuatf r;
	r.w= p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
	r.x= p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
	r.y= p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z;
	r.z= p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x;
	return r;
}

Application::Application()
	: mPipelineConfigured(false)
	, mSoundEffectContext(nullptr)
	, mSoundEffectPlayer(nullptr)
	, mGuiSys(OvrGuiSys::Create())
	, mLocale(nullptr)
	, sessionClient(this, std::make_unique<AndroidDiscoveryService>())
	, mDeviceContext(&GlobalGraphicsResources.renderPlatform)
	, clientRenderer(&resourceCreator, &resourceManagers,this,this,&clientDeviceState)
	, lobbyRenderer(&clientDeviceState)
	, resourceManagers(new OVRNodeManager)
	, resourceCreator(basist::transcoder_texture_format::cTFETC2)
{
	RedirectStdCoutCerr();

	sessionClient.SetResourceCreator(&resourceCreator);

	pthread_setname_np(pthread_self(), "SimulCaster_Application");
	mContext.setMessageHandler(Application::avsMessageHandler, this);

	if(enet_initialize() != 0)
	{
		OVR_FAIL("Failed to initialize ENET library");
	}

	if (AudioStream)
	{
#if defined( USE_AAUDIO )
		audioPlayer = new AA_AudioPlayer();
#else
		audioPlayer = new SL_AudioPlayer();
#endif
		audioPlayer->initializeAudioDevice();
	}

}

Application::~Application()
{
	mPipeline.deconfigure();

	mRefreshRates.clear();
	clientRenderer.ExitedVR();
	delete mSoundEffectPlayer;
	delete mSoundEffectContext;

	OvrGuiSys::Destroy(mGuiSys);

	sessionClient.Disconnect(REMOTEPLAY_TIMEOUT);
	enet_deinitialize();

	SAFE_DELETE(audioPlayer)
}

void Application::Configure(ovrSettings& settings )
{
	settings.CpuLevel = 0;
	settings.GpuLevel = 0;

	settings.EyeBufferParms.colorFormat = COLOR_8888;
	settings.EyeBufferParms.depthFormat = DEPTH_16;
	settings.EyeBufferParms.multisamples = 1;
	settings.TrackingSpace=VRAPI_TRACKING_SPACE_LOCAL_FLOOR;
	int res=vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_MULTIVIEW_AVAILABLE);
	useMultiview= (res == VRAPI_TRUE);
	//settings.TrackingTransform = VRAPI_TRACKING_TRANSFORM_SYSTEM_CENTER_EYE_LEVEL;
	settings.RenderMode = useMultiview?RENDERMODE_MULTIVIEW:RENDERMODE_STEREO;
}

void Application::EnteredVrMode(const ovrIntentType intentType, const char* intentFromPackage, const char* intentJSON, const char* intentURI )
{
	if (intentType != INTENT_LAUNCH)
		return;
	RedirectStdCoutCerr();

	std::string client_ini = LoadTextFile("client.ini");
	CSimpleIniA ini;

	SI_Error rc = ini.LoadData(client_ini.data(), client_ini.length());
	if (rc == SI_OK)
	{
		server_ip = ini.GetValue("", "SERVER_IP", "");
		server_discovery_port = ini.GetLongValue("", "SERVER_DISCOVERY_PORT",
												 REMOTEPLAY_SERVER_DISCOVERY_PORT);
	}
	else
	{
		std::cerr << "Create client.ini in assets directory to specify settings." << std::endl;
	}


	resourceCreator.Initialise((&GlobalGraphicsResources.renderPlatform),
							   scr::VertexBufferLayout::PackingStyle::INTERLEAVED);
	resourceCreator.AssociateResourceManagers(resourceManagers);

	//Default Effects
	scr::Effect::EffectCreateInfo ci;
	ci.effectName = "StandardEffects";
	GlobalGraphicsResources.defaultPBREffect.Create(&ci);

	//Default Sampler
	scr::Sampler::SamplerCreateInfo sci = {};
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

	OVR_LOG("%s | %s", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &GlobalGraphicsResources.maxFragTextureSlots);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &GlobalGraphicsResources.maxFragUniformBlocks);
	OVR_LOG("Fragment Texture Slots: %d, Fragment Uniform Blocks: %d",
			GlobalGraphicsResources.maxFragTextureSlots,
			GlobalGraphicsResources.maxFragUniformBlocks);

	//Setup Debug
	scc::SetupGLESDebug();

	const ovrJava *java = app->GetJava();
	mSoundEffectContext = new ovrSoundEffectContext(*java->Env, java->ActivityObject);
	mSoundEffectContext->Initialize(&app->GetFileSys());
	mSoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

	mLocale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");
	std::string fontName;
	GetLocale().GetString("@string/font_name", "efigs.fnt", fontName);

	clientRenderer.EnteredVR(app->GetOvrMobile(), java);
	mGuiSys->Init(app, *mSoundEffectPlayer, fontName.c_str(), &app->GetDebugLines());

	clientRenderer.mDecoder.setBackend(new VideoDecoderProxy(java->Env, this));


	//Set Lighting Cubemap Shader Resource
	scr::ShaderResourceLayout lightingCubemapLayout;
	lightingCubemapLayout.AddBinding(14,
									 scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,
									 scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	lightingCubemapLayout.AddBinding(15,
									 scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,
									 scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	lightingCubemapLayout.AddBinding(16,
									 scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,
									 scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	lightingCubemapLayout.AddBinding(17,
									 scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER,
									 scr::Shader::Stage::SHADER_STAGE_FRAGMENT);

	GlobalGraphicsResources.lightCubemapShaderResources.SetLayout(lightingCubemapLayout);
	GlobalGraphicsResources.lightCubemapShaderResources.AddImage(
			scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 14,
			"u_DiffuseCubemap", {clientRenderer.diffuseCubemapTexture->GetSampler()
								 , clientRenderer.diffuseCubemapTexture});
	GlobalGraphicsResources.lightCubemapShaderResources.AddImage(
			scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 15,
			"u_SpecularCubemap", {clientRenderer.specularCubemapTexture->GetSampler()
								  , clientRenderer.specularCubemapTexture});
	GlobalGraphicsResources.lightCubemapShaderResources.AddImage(
			scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 16,
			"u_RoughSpecularCubemap", {clientRenderer.mRoughSpecularTexture->GetSampler()
									   , clientRenderer.mRoughSpecularTexture});
	GlobalGraphicsResources.lightCubemapShaderResources.AddImage(
			scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 17,
			"u_LightsCubemap", {clientRenderer.mCubemapLightingTexture->GetSampler()
								, clientRenderer.mCubemapLightingTexture});


	int num_refresh_rates = vrapi_GetSystemPropertyInt(java,
													   VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
	mRefreshRates.resize(num_refresh_rates);
	vrapi_GetSystemPropertyFloatArray(java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,
									  mRefreshRates.data(), num_refresh_rates);

	if (num_refresh_rates > 0)
		vrapi_SetDisplayRefreshRate(app->GetOvrMobile(), mRefreshRates[num_refresh_rates - 1]);

	// Bind the delegates.

	controllers.SetToggleTexturesDelegate(
			std::bind(&ClientRenderer::ToggleTextures, &clientRenderer));
	controllers.SetToggleShowInfoDelegate(
			std::bind(&ClientRenderer::ToggleShowInfo, &clientRenderer));
	controllers.SetSetStickOffsetDelegate(
			std::bind(&ClientRenderer::SetStickOffset, &clientRenderer, std::placeholders::_1,
					  std::placeholders::_2));

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
	// we don't want local slide movements.
	mScene.SetMoveSpeed(1.0f);
	mScene.Frame(vrFrame,-1,false);
    clientRenderer.eyeSeparation=vrFrame.IPD;
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
	/*	if(keyCode == OVR_KEY_BACK && eventType == KEY_EVENT_SHORT_PRESS)
		{
			app->ShowConfirmQuitSystemUI();
			continue;
		}*/
	}

	// Try to find remote controller
	if((int)controllers.mControllerIDs[0] == 0)
	{
		controllers.InitializeController(app->GetOvrMobile());
	}
	controllers.Update(app->GetOvrMobile());
	clientDeviceState.originPose.position=*((const avs::vec3*)&mScene.GetFootPos());
	clientDeviceState.eyeHeight=mScene.GetEyeHeight();

	// Oculus Origin means where the headset's zero is in real space.

	// Handle networked session.
	if(sessionClient.IsConnected())
	{
		avs::DisplayInfo displayInfo = {1440, 1600};
		sessionClient.Frame(displayInfo, clientDeviceState.headPose, clientDeviceState.controllerPoses, receivedInitialPos, clientDeviceState.originPose, controllers.mLastControllerStates, clientRenderer.mDecoder.idrRequired(), vrFrame.RealTimeInSeconds);
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
	else
	{
		if (!sessionClient.HasDiscovered())
		{
			sessionClient.Discover("", REMOTEPLAY_CLIENT_DISCOVERY_PORT, server_ip.c_str(), server_discovery_port, remoteEndpoint);
		}
		if (sessionClient.HasDiscovered())
		{
			// if connect fails, restart discovery.
			if(!sessionClient.Connect(remoteEndpoint, REMOTEPLAY_TIMEOUT))
				sessionClient.Disconnect(0);
		}
	}

	//Get HMD Position/Orientation
	clientDeviceState.stickYaw=mScene.GetStickYaw();
	clientDeviceState.SetHeadPose(*((const avs::vec3 *)(&vrFrame.Tracking.HeadPose.Pose.Position)),*((const scr::quat *)(&vrFrame.Tracking.HeadPose.Pose.Orientation)));
	clientDeviceState.UpdateOriginPose();
	// Update video texture if we have any pending decoded frames.
	while(mNumPendingFrames > 0)
	{
		clientRenderer.mVideoSurfaceTexture->Update();
		--mNumPendingFrames;
	}

	// Process stream pipeline
	mPipeline.process();

	//Build frame
	ovrFrameResult res;

	mScene.GetFrameMatrices(vrFrame.FovX, vrFrame.FovY, res.FrameMatrices);
	mScene.GenerateFrameSurfaceList(res.FrameMatrices, res.Surfaces);

	// Update GUI systems after the app frame, but before rendering anything.
	mGuiSys->Frame(vrFrame, res.FrameMatrices.CenterView);
	// The camera should be where our head is. But when rendering, the camera is in OVR space, so:
	GlobalGraphicsResources.scrCamera->UpdatePosition(clientDeviceState.headPose.position);

    std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
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
	clientRenderer.CopyToCubemaps(mDeviceContext);
	// Append video surface
    clientRenderer.RenderVideo(mDeviceContext,res);

	if(sessionClient.IsConnected())
	{
		clientRenderer.Render(vrFrame,mGuiSys);
	}
	else
	{
		res.ClearColorBuffer=true;
		res.ClearDepthBuffer=true;
		lobbyRenderer.Render(mGuiSys);
	};

	//Append SCR Nodes to surfaces.
	GL_CheckErrors("Frame: Pre-SCR");
	uint32_t time_elapsed=(uint32_t)(vrFrame.DeltaSeconds*1000.0f);
	resourceManagers.Update(time_elapsed);
	resourceCreator.Update(time_elapsed);

	//Move the hands before they are drawn.
	UpdateHandObjects();
	clientRenderer.RenderLocalNodes(res);
	GL_CheckErrors("Frame: Post-SCR");

	// Append GuiSys surfaces. This should always be the last item to append the render list.
	mGuiSys->AppendSurfaceList(res.FrameMatrices.CenterView, &res.Surfaces);
/*	if(useMultiview)
	{
		// Initialize the FrameParms.
		FrameParms = vrapi_DefaultFrameParms( app->GetJava(), VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), NULL );
		for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
		{
			res.Layers[0].Textures[eye].ColorTextureSwapChain = vrFrame.ColorTextureSwapChain[eye];
			//FrameParms.Layers[0].Textures[eye].DepthTextureSwapChain = vrFrame.DepthTextureSwapChain[eye];
			res.Layers[0].Textures[eye].TextureSwapChainIndex = vrFrame.TextureSwapChainIndex;

			res.Layers[0].Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
			res.Layers[0].Textures[eye].HeadPose = vrFrame.Tracking.HeadPose;
		}

		//FrameParms.ExternalVelocity = mScene.GetExternalVelocity();
		res.Layers[0].Flags = VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	}*/
	return res;
}


void Application::UpdateHandObjects()
{
	uint32_t deviceIndex = 0;
	ovrInputCapabilityHeader capsHeader;
	//Poll controller state from the Oculus API.
	while(vrapi_EnumerateInputDevices(app->GetOvrMobile(), deviceIndex, &capsHeader) >= 0)
	{
		if(capsHeader.Type == ovrControllerType_TrackedRemote)
		{
			ovrTracking remoteState;
			if(vrapi_GetInputTrackingState(app->GetOvrMobile(), capsHeader.DeviceID, 0, &remoteState) >= 0)
			{
				if(deviceIndex < 2)
				{
					clientDeviceState.SetControllerPose(deviceIndex,*((const avs::vec3 *)(&remoteState.HeadPose.Pose.Position)),*((const scr::quat *)(&remoteState.HeadPose.Pose.Orientation)));
				}
				else
				{
					break;
				}
			}
		}
		++deviceIndex;
	}


	std::shared_ptr<scr::Node> body = resourceManagers.mNodeManager->GetBody();
	if(body)
	{
		body->UpdateModelMatrix(clientDeviceState.headPose.position, clientDeviceState.headPose.orientation, body->GetGlobalTransform().m_Scale);
	}

	std::shared_ptr<scr::Node> rightHand = resourceManagers.mNodeManager->GetRightHand();
	if(rightHand)
	{
		avs::vec3 newPosition = clientDeviceState.controllerPoses[0].position;
		scr::quat newRotation = scr::quat(clientDeviceState.controllerPoses[0].orientation) * HAND_ROTATION_DIFFERENCE;
		rightHand->UpdateModelMatrix(newPosition, newRotation, rightHand->GetGlobalTransform().m_Scale);
	}

	std::shared_ptr<scr::Node> leftHand = resourceManagers.mNodeManager->GetLeftHand();
	if(leftHand)
	{
		avs::vec3 newPosition = clientDeviceState.controllerPoses[1].position;
		scr::quat newRotation = scr::quat(clientDeviceState.controllerPoses[1].orientation) * HAND_ROTATION_DIFFERENCE;
		leftHand->UpdateModelMatrix(newPosition, newRotation, leftHand->GetGlobalTransform().m_Scale);
	}
}

void Application::OnVideoStreamChanged(const char* server_ip, const avs::SetupCommand& setupCommand, avs::Handshake& handshake)
{
    const avs::VideoConfig& videoConfig = setupCommand.video_config;
	if(!mPipelineConfigured)
    {
		OVR_WARN("VIDEO STREAM CHANGED: %d %d %d, cubemap %d", setupCommand.port,
                 videoConfig.video_width, videoConfig.video_height,
                 videoConfig.colour_cubemap_size);

		sessionClient.SetPeerTimeout(setupCommand.idle_connection_timeout);

		std::vector<avs::NetworkSourceStream> streams = { {20} };
		if (AudioStream)
		{
			streams.push_back({ 40 });
		}
		if (GeoStream)
		{
			streams.push_back({ 60 });
		}

        avs::NetworkSourceParams sourceParams;
        sourceParams.connectionTimeout = setupCommand.idle_connection_timeout;
        sourceParams.localPort = setupCommand.port + 1;
        sourceParams.remoteIP = sessionClient.GetServerIP().c_str();
        sourceParams.remotePort = setupCommand.port;

		if (!clientRenderer.mNetworkSource.configure(std::move(streams), sourceParams))
		{
			OVR_WARN("OnVideoStreamChanged: Failed to configure network source node.");
			return;
		}
	    clientRenderer.mNetworkSource.setDebugStream(setupCommand.debug_stream);
	    clientRenderer.mNetworkSource.setDebugNetworkPackets(setupCommand.debug_network_packets);
	    clientRenderer.mNetworkSource.setDoChecksums(setupCommand.do_checksums);

	    mPipeline.add(&clientRenderer.mNetworkSource);

		clientRenderer.mVideoTagData2DArray.clear();
		clientRenderer.mVideoTagData2DArray.resize(clientRenderer.MAX_TAG_DATA_COUNT);
		clientRenderer.videoTagDataCubeArray.clear();
		clientRenderer.videoTagDataCubeArray.resize(clientRenderer.MAX_TAG_DATA_COUNT);

		avs::DecoderParams decoderParams = {};
		decoderParams.codec             = videoConfig.videoCodec;
		decoderParams.decodeFrequency   = avs::DecodeFrequency::NALUnit;
		decoderParams.prependStartCodes = false;
		decoderParams.deferDisplay      = false;

		size_t stream_width  = videoConfig.video_width;
		size_t stream_height = videoConfig.video_height;
		// test
		auto f = std::bind(&ClientRenderer::OnReceiveVideoTagData, &clientRenderer, std::placeholders::_1, std::placeholders::_2);
		if (!clientRenderer.mDecoder.configure(avs::DeviceHandle(), stream_width, stream_height, decoderParams, 20, f))
		{
			OVR_WARN("OnVideoStreamChanged: Failed to configure decoder node");
			clientRenderer.mNetworkSource.deconfigure();
			return;
		}
		{
			scr::Texture::TextureCreateInfo textureCreateInfo = {};
			textureCreateInfo.externalResource = true;
			textureCreateInfo.slot   = scr::Texture::Slot::NORMAL;
			textureCreateInfo.format = scr::Texture::Format::RGBA8;
			textureCreateInfo.type   = scr::Texture::Type::TEXTURE_2D_EXTERNAL_OES;
			textureCreateInfo.height = videoConfig.video_height;
			textureCreateInfo.width  = videoConfig.video_width;

			clientRenderer.mVideoTexture->Create(textureCreateInfo);
			((scc::GL_Texture *) (clientRenderer.mVideoTexture.get()))->SetExternalGlTexture(
					clientRenderer.mVideoSurfaceTexture->GetTextureId());

		}

		mSurface.configure(new VideoSurface(clientRenderer.mVideoSurfaceTexture));

		clientRenderer.mVideoQueue.configure(200000, 16, "VideoQueue");

		avs::Node::link(clientRenderer.mNetworkSource, clientRenderer.mVideoQueue);
		avs::Node::link(clientRenderer.mVideoQueue, clientRenderer.mDecoder);
		mPipeline.link({ &clientRenderer.mDecoder, &mSurface });


		// Audio
		if (AudioStream)
		{
			avsAudioDecoder.configure(40);
			sca::AudioParams audioParams;
			audioParams.codec = sca::AudioCodec::PCM;
			audioParams.numChannels = 2;
			audioParams.sampleRate = 48000;
			audioParams.bitsPerSample = 32;
			// This will be deconfigured automatically when the pipeline is deconfigured.
			audioPlayer->configure(audioParams);
			audioStreamTarget.reset(new sca::AudioStreamTarget(audioPlayer));
			avsAudioTarget.configure(audioStreamTarget.get());
			clientRenderer.mAudioQueue.configure(4096, 120, "AudioQueue");

			avs::Node::link(clientRenderer.mNetworkSource, clientRenderer.mAudioQueue);
			avs::Node::link(clientRenderer.mAudioQueue, avsAudioDecoder);
			mPipeline.link({ &avsAudioDecoder, &avsAudioTarget });

			// Audio Input
			if (setupCommand.audio_input_enabled)
            {
                sca::NetworkSettings networkSettings =
                {
						setupCommand.port + 1,
                        server_ip,
						setupCommand.port,
                        static_cast<int32_t>(handshake.maxBandwidthKpS),
                        static_cast<int32_t>(handshake.udpBufferSize),
                        setupCommand.requiredLatencyMs,
						(int32_t)setupCommand.idle_connection_timeout
                };

                mNetworkPipeline.reset(new sca::NetworkPipeline());
				mAudioInputQueue.configure(4096, 120, "AudioInputQueue");
				mNetworkPipeline->initialise(networkSettings, &mAudioInputQueue);

				// Callback called on separate thread when recording buffer is full
				auto f = [this](const uint8_t * data, size_t dataSize)->void
				{
					size_t bytesWritten;
					if(mAudioInputQueue.write(nullptr, data, dataSize, bytesWritten))
					{
						mNetworkPipeline->process();
					}
				};
				audioPlayer->startRecording(f);
            }
		}

		if (GeoStream)
		{
			avsGeometryDecoder.configure(60, &geometryDecoder);
			avsGeometryTarget.configure(&resourceCreator);
			clientRenderer.mGeometryQueue.configure(10000, 200, "GeometryQueue");
			mPipeline.link({&clientRenderer.mNetworkSource, &avsGeometryDecoder, &avsGeometryTarget});

			avs::Node::link(clientRenderer.mNetworkSource, clientRenderer.mGeometryQueue);
			avs::Node::link(clientRenderer.mGeometryQueue, avsGeometryDecoder);
			mPipeline.link({ &avsGeometryDecoder, &avsGeometryTarget });
		}
		//GL_CheckErrors("Pre-Build Cubemap");
		clientRenderer.OnVideoStreamChanged(videoConfig);

		mPipelineConfigured = true;
	}

    handshake.startDisplayInfo.width = 1440;
    handshake.startDisplayInfo.height = 1600;
    handshake.framerate = 60;
    handshake.FOV = 110;
    handshake.isVR = true;
	handshake.udpBufferSize = static_cast<uint32_t>(clientRenderer.mNetworkSource.getSystemBufferSize());
	handshake.maxBandwidthKpS = 10*handshake.udpBufferSize * static_cast<uint32_t>(handshake.framerate);
	handshake.axesStandard = avs::AxesStandard::GlStyle;
	handshake.MetresPerUnit = 1.0f;
	handshake.usingHands = true;

	clientRenderer.mIsCubemapVideo = setupCommand.video_config.use_cubemap;

	clientRenderer.lastSetupCommand = setupCommand;
}

void Application::OnVideoStreamClosed()
{
	OVR_WARN("VIDEO STREAM CLOSED");

	mPipeline.deconfigure();
	mPipeline.reset();
	mPipelineConfigured = false;

    receivedInitialPos = false;
}

void Application::OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand)
{
	if(!mPipelineConfigured)
	{
		return;
	}

	clientRenderer.OnVideoStreamChanged(reconfigureVideoCommand.video_config);
    WARN("VIDEO STREAM RECONFIGURED: clr %d x %d dpth %d x %d", clientRenderer.videoConfig.video_width, clientRenderer.videoConfig.video_height
    , clientRenderer.videoConfig.depth_width, clientRenderer.videoConfig.depth_height);
}

bool Application::OnNodeEnteredBounds(avs::uid id)
{
    return resourceManagers.mNodeManager->ShowNode(id);
}

bool Application::OnNodeLeftBounds(avs::uid id)
{
	return resourceManagers.mNodeManager->HideNode(id);
}

std::vector<uid> Application::GetGeometryResources()
{
    return resourceManagers.GetAllResourceIDs();
}

void Application::ClearGeometryResources()
{
    resourceManagers.Clear();
}

void Application::SetVisibleNodes(const std::vector<avs::uid>& visibleNodes)
{
    resourceManagers.mNodeManager->SetVisibleNodes(visibleNodes);
}

void Application::UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList)
{
	resourceManagers.mNodeManager->UpdateNodeMovement(updateList);
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

const scr::Effect::EffectPassCreateInfo* Application::BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl
		, const scr::ShaderSystem::PipelineCreateInfo *pipelineCreateInfo,  const std::vector<scr::ShaderResource>& shaderResources)
{
	if(GlobalGraphicsResources.defaultPBREffect.HasEffectPass(effectPassName))
		return GlobalGraphicsResources.defaultPBREffect.GetEffectPassCreateInfo(effectPassName);

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

	GlobalGraphicsResources.defaultPBREffect.CreatePass(&ci);
	GlobalGraphicsResources.defaultPBREffect.LinkShaders(effectPassName, shaderResources);

	return GlobalGraphicsResources.defaultPBREffect.GetEffectPassCreateInfo(effectPassName);
}

std::string Application::LoadTextFile(const char *filename)
{
	std::vector<uint8_t> outBuffer;
	std::string str = "apk:///assets/";
	str += filename;
	if (!app)
	{

	}
	else if (app->GetFileSys().ReadFile(str.c_str(), outBuffer))
	{
		if (outBuffer.back() != '\0')
			outBuffer.push_back(
					'\0'); //Append Null terminator character. ReadFile() does return a null terminated string, apparently!
		return std::string((const char *) outBuffer.data());
	}
	return "";
}