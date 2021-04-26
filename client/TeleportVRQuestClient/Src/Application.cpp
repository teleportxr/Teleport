// (C) Copyright 2018-2021 Simul Software Ltd

#include "Application.h"
#include <sstream>
#include "GLESDebug.h"
#include "AndroidDiscoveryService.h"
#include "OVRNodeManager.h"
#include "VideoSurface.h"
#include <libavstream/common.hpp>
#include "Config.h"
#include "Log.h"
#include "SimpleIni.h"

#if defined( USE_AAUDIO )
#include "SCR_Class_AAudio_Impl/AA_AudioPlayer.h"
#else

#include "SCR_Class_SL_Impl/SL_AudioPlayer.h"

#endif

using namespace OVRFW;


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
		  , mPipelineConfigured(false)
		  , mSoundEffectPlayer(nullptr)
		  , mGuiSys(nullptr)
		  , sessionClient(this, std::make_unique<AndroidDiscoveryService>())
		  , clientRenderer(&resourceCreator, &resourceManagers, this, this, &clientDeviceState,&controllers)
		  , lobbyRenderer(&clientDeviceState)
		  , resourceManagers(new OVRNodeManager)
		  , resourceCreator(basist::transcoder_texture_format::cTFETC2)
{
	RedirectStdCoutCerr();

	sessionClient.SetResourceCreator(&resourceCreator);

	pthread_setname_np(pthread_self(), "SimulCaster_Application");
	mContext.setMessageHandler(Application::avsMessageHandler, this);

	if (enet_initialize() != 0)
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
	sessionClient.Disconnect(REMOTEPLAY_TIMEOUT);
	enet_deinitialize();
	SAFE_DELETE(audioPlayer)

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
		server_ip = ini.GetValue("", "SERVER_IP", "");
		server_discovery_port = ini.GetLongValue("", "SERVER_DISCOVERY_PORT",
												 REMOTEPLAY_SERVER_DISCOVERY_PORT);
		client_service_port = ini.GetLongValue("", "CLIENT_SERVICE_PORT",
												 REMOTEPLAY_CLIENT_SERVICE_PORT);
		client_streaming_port = ini.GetLongValue("", "CLIENT_STREAMING_PORT",
												 REMOTEPLAY_CLIENT_STREAMING_PORT);
		return true;
	}
	else
	{
		std::cerr << "Create client.ini in assets directory to specify settings." << std::endl;
		return false;
	}
}

bool Application::AppInit(const OVRFW::ovrAppContext *context)
{
	RedirectStdCoutCerr();
	const ovrJava &jj = *(reinterpret_cast<const ovrJava *>(context->ContextForVrApi()));
	const xrJava ctx = JavaContextConvert(jj);
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


	SurfaceRender.Init();

	startTime = GetTimeInSeconds();

	EnteredVrMode();
	return true;
}

void Application::EnteredVrMode()
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	mDeviceContext.reset(new scc::GL_DeviceContext(&globalGraphicsResources.renderPlatform));
	resourceCreator.Initialise((&globalGraphicsResources.renderPlatform),
							   scr::VertexBufferLayout::PackingStyle::INTERLEAVED);
	resourceCreator.AssociateResourceManagers(resourceManagers);

	//Default Effects
	scr::Effect::EffectCreateInfo ci;
	ci.effectName = "StandardEffects";
	globalGraphicsResources.defaultPBREffect.Create(&ci);

	//Default Sampler
	scr::Sampler::SamplerCreateInfo sci = {};
	sci.wrapU = scr::Sampler::Wrap::REPEAT;
	sci.wrapV = scr::Sampler::Wrap::REPEAT;
	sci.wrapW = scr::Sampler::Wrap::REPEAT;
	sci.minFilter = scr::Sampler::Filter::LINEAR;
	sci.magFilter = scr::Sampler::Filter::LINEAR;

	globalGraphicsResources.sampler = globalGraphicsResources.renderPlatform.InstantiateSampler();
	globalGraphicsResources.sampler->Create(&sci);

	sci.minFilter = scr::Sampler::Filter::MIPMAP_LINEAR;
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

	globalGraphicsResources.lightCubemapShaderResources.SetLayout(lightingCubemapLayout);
	globalGraphicsResources.lightCubemapShaderResources.AddImage(
			scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 14,
			"u_SpecularCubemap", {clientRenderer.specularCubemapTexture->GetSampler()
								  , clientRenderer.specularCubemapTexture});
/*	globalGraphicsResources.lightCubemapShaderResources.AddImage(
			scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 15,
			"u_LightsCubemap", {clientRenderer.mCubemapLightingTexture->GetSampler()
								, clientRenderer.mCubemapLightingTexture});
	globalGraphicsResources.lightCubemapShaderResources.AddImage(
			scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 16,
			"u_DiffuseCubemap", {clientRenderer.diffuseCubemapTexture->GetSampler()
								 , clientRenderer.diffuseCubemapTexture});*/

	scr::ShaderResourceLayout tagBufferLayout;
	tagBufferLayout.AddBinding(1,
									 scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER,
									 scr::Shader::Stage::SHADER_STAGE_FRAGMENT);
	globalGraphicsResources.tagShaderResource.SetLayout(tagBufferLayout);
	globalGraphicsResources.tagShaderResource.AddBuffer(scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER,1,"TagDataCube_ssbo",{
			globalGraphicsResources.mTagDataBuffer.get()});
	//useMultiview=(vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_MULTIVIEW_AVAILABLE ) == VRAPI_TRUE);
	int num_refresh_rates = vrapi_GetSystemPropertyInt(java,
													   VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
	mRefreshRates.resize(num_refresh_rates);
	vrapi_GetSystemPropertyFloatArray(java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,
									  mRefreshRates.data(), num_refresh_rates);

	//if (num_refresh_rates > 0)
	//	vrapi_SetDisplayRefreshRate(app->GetOvrMobile(), mRefreshRates[num_refresh_rates - 1]);

	// Bind the delegates.

	controllers.SetCycleShaderModeDelegate(std::bind(&ClientRenderer::CycleShaderMode, &clientRenderer));
	controllers.SetCycleOSDDelegate(std::bind(&ClientRenderer::CycleOSD, &clientRenderer));
	controllers.SetSetStickOffsetDelegate(std::bind(&ClientRenderer::SetStickOffset, &clientRenderer, std::placeholders::_1, std::placeholders::_2));
}

void Application::AppShutdown(const OVRFW::ovrAppContext *)
{
	ALOGV("AppShutdown - enter");
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
	mScene.Frame(vrFrame,-1,false);
	clientRenderer.eyeSeparation=vrFrame.IPD;
	GLCheckErrorsWithTitle("Frame: Start");
	// Try to find remote controller
	if((int)controllers.mControllerIDs[0] == 0)
	{
		controllers.InitializeController(GetSessionObject(),0);
	}
	if((int)controllers.mControllerIDs[1] == 0)
	{
		controllers.InitializeController(GetSessionObject(),1);
	}
	controllers.Update(GetSessionObject());
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
	clientDeviceState.SetHeadPose(*((const avs::vec3 *)(&vrFrame.HeadPose.Translation)),*((const scr::quat *)(&vrFrame.HeadPose.Rotation)));
	clientDeviceState.UpdateOriginPose();
	// Update video texture if we have any pending decoded frames.
	while(mNumPendingFrames > 0)
	{
		clientRenderer.mVideoSurfaceTexture->Update();
		--mNumPendingFrames;
	}

	// Process stream pipeline
	mPipeline.process();

	return OVRFW::ovrApplFrameOut();
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
			//out.FrameMatrices.CenterView = CenterEyeViewMatrix;
			for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
			{
				out.FrameMatrices.EyeView[eye] = in.Eye[eye].ViewMatrix;
				// Calculate projection matrix using custom near plane value.
				out.FrameMatrices.EyeProjection[eye] = ovrMatrix4f_CreateProjectionFov(
						SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, 0.0f, 0.0f, 0.1f,
						7.0f);
			}


			///	worldLayer.Header.Flags |=
			/// VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

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

void Application::Render(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out)
{
	if(!VideoDecoderProxy::IsJNIInitialized())
		return ;
//Build frame
	mScene.GetFrameMatrices(SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
	mScene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
// The camera should be where our head is. But when rendering, the camera is in OVR space, so:
	globalGraphicsResources.scrCamera->UpdatePosition(clientDeviceState.headPose.position);

	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;

	GLCheckErrorsWithTitle("Frame: Pre-Cubemap");
	clientRenderer.CopyToCubemaps(*mDeviceContext);


//Append SCR Nodes to surfaces.
	GLCheckErrorsWithTitle("Frame: Pre-SCR");
	float time_elapsed = in.DeltaSeconds * 1000.0f;
	resourceManagers.Update(time_elapsed);
	resourceCreator.Update(time_elapsed);

//Move the hands before they are drawn.
	UpdateHandObjects();
	clientRenderer.RenderLocalNodes(out);
	if (sessionClient.IsConnected())
	{
		clientRenderer.Render(in, mGuiSys);
// Append video surface
		clientRenderer.RenderVideo(*mDeviceContext, out);
	}
	else
	{
		lobbyRenderer.Render(mGuiSys);
	};
	GLCheckErrorsWithTitle("Frame: Post-SCR");

}

void Application::UpdateHandObjects()
{
	uint32_t deviceIndex = 0;
	ovrInputCapabilityHeader capsHeader;
	//Poll controller state from the Oculus API.
	while(vrapi_EnumerateInputDevices(GetSessionObject(), deviceIndex, &capsHeader) >= 0)
	{
		if(capsHeader.Type == ovrControllerType_TrackedRemote)
		{
			ovrTracking remoteState;
			if(vrapi_GetInputTrackingState(GetSessionObject(), capsHeader.DeviceID, 0, &remoteState) >= 0)
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
			body->SetLocalPosition(clientDeviceState.headPose.position + bodyOffsetFromHead);

			//Calculate rotation angle on y-axis, and use to create new quaternion that only rotates the body on the y-axis.
			float angle = std::atan2(clientDeviceState.headPose.orientation.y, clientDeviceState.headPose.orientation.w);
			scr::quat yRotation(0.0f, std::sin(angle), 0.0f, std::cos(angle));
			body->SetLocalRotation(yRotation);
	}

	std::shared_ptr<scr::Node> rightHand = resourceManagers.mNodeManager->GetRightHand();
	if(rightHand)
	{
			rightHand->SetLocalPosition(clientDeviceState.controllerPoses[0].position);
			rightHand->SetLocalRotation(clientDeviceState.controllerRelativePoses[0].orientation);
	}

	std::shared_ptr<scr::Node> leftHand = resourceManagers.mNodeManager->GetLeftHand();
	if(leftHand)
	{
			leftHand->SetLocalPosition(clientDeviceState.controllerPoses[1].position);
			leftHand->SetLocalRotation(clientDeviceState.controllerRelativePoses[1].orientation);
	}
}

void Application::AppRenderEye(
		const OVRFW::ovrApplFrameIn &vrFrame, OVRFW::ovrRendererOutput &out, int eye)
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

void Application::OnVideoStreamChanged(const char *server_ip, const avs::SetupCommand &setupCommand, avs::Handshake &handshake)
{
	const avs::VideoConfig &videoConfig = setupCommand.video_config;
	if (!mPipelineConfigured)
	{
		OVR_WARN("VIDEO STREAM CHANGED: server port %d %d %d, cubemap %d", setupCommand.server_streaming_port,
				 videoConfig.video_width, videoConfig.video_height,
				 videoConfig.colour_cubemap_size);

		sessionClient.SetPeerTimeout(setupCommand.idle_connection_timeout);

		std::vector<avs::NetworkSourceStream> streams = {{20}};
		if (AudioStream)
		{
			streams.push_back({40});
		}
		if (GeoStream)
		{
			streams.push_back({60});
		}

		avs::NetworkSourceParams sourceParams;
		sourceParams.connectionTimeout = setupCommand.idle_connection_timeout;
		sourceParams.localPort = client_streaming_port;
		sourceParams.remoteIP = sessionClient.GetServerIP().c_str();
		sourceParams.remotePort = setupCommand.server_streaming_port;

		bodyOffsetFromHead = setupCommand.bodyOffsetFromHead;
		avs::ConvertPosition(setupCommand.axesStandard, avs::AxesStandard::GlStyle, bodyOffsetFromHead);

		if (!clientRenderer.mNetworkSource.configure(std::move(streams), sourceParams))
		{
			OVR_WARN("OnVideoStreamChanged: Failed to configure network source node.");
			return;
		}
		clientRenderer.mNetworkSource.setDebugStream(setupCommand.debug_stream);
		clientRenderer.mNetworkSource.setDebugNetworkPackets(setupCommand.debug_network_packets);
		clientRenderer.mNetworkSource.setDoChecksums(setupCommand.do_checksums);

		mPipeline.add(&clientRenderer.mNetworkSource);

		clientRenderer.videoTagDataCubeArray.clear();
		clientRenderer.videoTagDataCubeArray.resize(clientRenderer.MAX_TAG_DATA_COUNT);

		avs::DecoderParams decoderParams = {};
		decoderParams.codec = videoConfig.videoCodec;
		decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
		decoderParams.prependStartCodes = false;
		decoderParams.deferDisplay = false;

		size_t stream_width = videoConfig.video_width;
		size_t stream_height = videoConfig.video_height;
		// test
		auto f = std::bind(&ClientRenderer::OnReceiveVideoTagData, &clientRenderer,
						   std::placeholders::_1, std::placeholders::_2);
		if (!clientRenderer.mDecoder.configure(avs::DeviceHandle(), stream_width, stream_height,
											   decoderParams, 20, f))
		{
			OVR_WARN("OnVideoStreamChanged: Failed to configure decoder node");
			clientRenderer.mNetworkSource.deconfigure();
			return;
		}
		{
			scr::Texture::TextureCreateInfo textureCreateInfo = {};
			textureCreateInfo.externalResource = true;
			textureCreateInfo.slot = scr::Texture::Slot::NORMAL;
			textureCreateInfo.format = scr::Texture::Format::RGBA8;
			textureCreateInfo.type = scr::Texture::Type::TEXTURE_2D_EXTERNAL_OES;
			textureCreateInfo.height = videoConfig.video_height;
			textureCreateInfo.width = videoConfig.video_width;

			clientRenderer.mVideoTexture->Create(textureCreateInfo);
			((scc::GL_Texture *) (clientRenderer.mVideoTexture.get()))->SetExternalGlTexture(
					clientRenderer.mVideoSurfaceTexture->GetTextureId());

		}

		mSurface.configure(new VideoSurface(clientRenderer.mVideoSurfaceTexture));

		clientRenderer.mVideoQueue.configure(200000, 16, "VideoQueue");

		avs::Node::link(clientRenderer.mNetworkSource, clientRenderer.mVideoQueue);
		avs::Node::link(clientRenderer.mVideoQueue, clientRenderer.mDecoder);
		mPipeline.link({&clientRenderer.mDecoder, &mSurface});


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
			mPipeline.link({&avsAudioDecoder, &avsAudioTarget});

			// Audio Input
			if (setupCommand.audio_input_enabled)
			{
				sca::NetworkSettings networkSettings =
						{
								setupCommand.server_streaming_port + 1, server_ip, setupCommand.server_streaming_port
								, static_cast<int32_t>(handshake.maxBandwidthKpS)
								, static_cast<int32_t>(handshake.udpBufferSize)
								, setupCommand.requiredLatencyMs
								, (int32_t) setupCommand.idle_connection_timeout
						};

				mNetworkPipeline.reset(new sca::NetworkPipeline());
				mAudioInputQueue.configure(4096, 120, "AudioInputQueue");
				mNetworkPipeline->initialise(networkSettings, &mAudioInputQueue);

				// Callback called on separate thread when recording buffer is full
				auto f = [this](const uint8_t *data, size_t dataSize) -> void
				{
					size_t bytesWritten;
					if (mAudioInputQueue.write(nullptr, data, dataSize, bytesWritten))
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
			mPipeline.link(
					{&clientRenderer.mNetworkSource, &avsGeometryDecoder, &avsGeometryTarget});

			avs::Node::link(clientRenderer.mNetworkSource, clientRenderer.mGeometryQueue);
			avs::Node::link(clientRenderer.mGeometryQueue, avsGeometryDecoder);
			mPipeline.link({&avsGeometryDecoder, &avsGeometryTarget});
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
	handshake.maxBandwidthKpS =
			10 * handshake.udpBufferSize * static_cast<uint32_t>(handshake.framerate);
	handshake.axesStandard = avs::AxesStandard::GlStyle;
	handshake.MetresPerUnit = 1.0f;
	handshake.usingHands = true;
	handshake.maxLightsSupported=4;
	handshake.clientStreamingPort=client_streaming_port;
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

void Application::OnReconfigureVideo(const avs::ReconfigureVideoCommand &reconfigureVideoCommand)
{
	if (!mPipelineConfigured)
	{
		return;
	}

	clientRenderer.OnVideoStreamChanged(reconfigureVideoCommand.video_config);
	WARN("VIDEO STREAM RECONFIGURED: clr %d x %d dpth %d x %d",
		 clientRenderer.videoConfig.video_width, clientRenderer.videoConfig.video_height,
		 clientRenderer.videoConfig.depth_width, clientRenderer.videoConfig.depth_height);
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

void Application::SetVisibleNodes(const std::vector<avs::uid> &visibleNodes)
{
	resourceManagers.mNodeManager->SetVisibleNodes(visibleNodes);
}

void Application::UpdateNodeMovement(const std::vector<avs::MovementUpdate> &updateList)
{
	resourceManagers.mNodeManager->UpdateNodeMovement(updateList);
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

const scr::Effect::EffectPassCreateInfo *
Application::BuildEffectPass(const char *effectPassName, scr::VertexBufferLayout *vbl
							 , const scr::ShaderSystem::PipelineCreateInfo *pipelineCreateInfo
							 , const std::vector<scr::ShaderResource> &shaderResources)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	if (globalGraphicsResources.defaultPBREffect.HasEffectPass(effectPassName))
	{
		return globalGraphicsResources.defaultPBREffect.GetEffectPassCreateInfo(effectPassName);
	}

	scr::ShaderSystem::PassVariables pv;
	pv.mask = false;
	pv.reverseDepth = false;
	pv.msaa = false;

	scr::ShaderSystem::Pipeline gp(&globalGraphicsResources.renderPlatform, pipelineCreateInfo);

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
	vs.extentX = (uint32_t) vs.x;
	vs.extentY = (uint32_t) vs.y;

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
	cbs.colorBlendOp = scr::Effect::BlendOp::ADD;
	cbs.srcAlphaBlendFactor = scr::Effect::BlendFactor::ONE;
	cbs.dstAlphaBlendFactor = scr::Effect::BlendFactor::ZERO;
	cbs.alphaBlendOp = scr::Effect::BlendOp::ADD;

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
			outBuffer.push_back(
					'\0');
		} //Append Null terminator character. ReadFile() does return a null terminated string, apparently!
		return std::string((const char *) outBuffer.data());
	}
	return "";
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
