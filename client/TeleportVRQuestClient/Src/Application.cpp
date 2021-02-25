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
	JNIEXPORT jlong Java_com_oculus_sdk_vrcubeworldfw_MainActivity_nativeInitFromJava(JNIEnv *jni)
	{
		VideoDecoderProxy::InitializeJNI(jni);
		return 0;
	}
} // extern "C"


static const char VERTEX_SHADER[] = R"glsl(
in vec3 Position;
in vec4 VertexColor;
in mat4 VertexTransform;
out vec4 fragmentColor;
void main()
{
	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( VertexTransform * vec4( Position * 0.1, 1.0 ) ) );
	fragmentColor = VertexColor;
}
)glsl";

static const char FRAGMENT_SHADER[] = R"glsl(
in lowp vec4 fragmentColor;
void main()
{
	gl_FragColor = fragmentColor;
}
)glsl";

// setup Cube
struct ovrCubeVertices
{
	Vector3f positions[8];
	Vector4f colors[8];
};

static ovrCubeVertices cubeVertices = {
		// positions
		{
				Vector3f(-1.0f, +1.0f, -1.0f)   , Vector3f(+1.0f, +1.0f, -1.0f)   , Vector3f(+1.0f,
																							 +1.0f,
																							 +1.0f), Vector3f(
				-1.0f, +1.0f, +1.0f)   , // top
				Vector3f(-1.0f, -1.0f, -1.0f)                            , Vector3f(-1.0f, -1.0f,
																					+1.0f), Vector3f(
				+1.0f, -1.0f, +1.0f)   , Vector3f(+1.0f, -1.0f, -1.0f) // bottom
		}
		,
		// colors
		{       Vector4f(1.0f, 0.0f, 1.0f, 1.0f), Vector4f(0.0f, 1.0f, 0.0f, 1.0f), Vector4f(0.0f,
																							 0.0f,
																							 1.0f,
																							 1.0f) , Vector4f(
				1.0f, 0.0f, 0.0f, 1.0f), Vector4f(0.0f, 0.0f, 1.0f, 1.0f), Vector4f(0.0f, 1.0f,
																					0.0f,
																					1.0f) , Vector4f(
				1.0f, 0.0f, 1.0f, 1.0f), Vector4f(1.0f, 0.0f, 0.0f, 1.0f)}
		,};

static const unsigned short cubeIndices[36] = {
		0, 2, 1, 2, 0, 3, // top
		4, 6, 5, 6, 4, 7, // bottom
		2, 6, 7, 7, 1, 2, // right
		0, 4, 5, 5, 3, 0, // left
		3, 5, 6, 6, 2, 3, // front
		0, 1, 7, 7, 4, 0 // back
};


Application::Application()
		: ovrAppl(0, 0, CPU_LEVEL, GPU_LEVEL, true /* useMultiView */), mSoundEffectPlayer(nullptr)
		  , Locale(nullptr)
		  ,mGuiSys(nullptr)
		  , Random(2)
		  , mPipelineConfigured(false)
		  , sessionClient(this, std::make_unique<AndroidDiscoveryService>())
		  , mDeviceContext(&GlobalGraphicsResources.renderPlatform)
		  , clientRenderer(&resourceCreator, &resourceManagers, this, this, &clientDeviceState)
		  , lobbyRenderer(&clientDeviceState)
		  , resourceManagers(new OVRNodeManager)
		  , resourceCreator(basist::transcoder_texture_format::cTFETC2)
{
	CenterEyeViewMatrix = ovrMatrix4f_CreateIdentity();
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

	GlProgram::Free(Program);
	Cube.Free();
	GL(glDeleteBuffers(1, &InstanceTransformBuffer));

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

	// Create the program.
	Program = GlProgram::Build(VERTEX_SHADER, FRAGMENT_SHADER, nullptr, 0);
	VertexTransformAttribute = glGetAttribLocation(Program.Program, "VertexTransform");

	// Create the cube.
	VertexAttribs attribs;
	attribs.position.resize(8);
	attribs.color.resize(8);
	for (int i = 0; i < 8; i++)
	{
		attribs.position[i] = cubeVertices.positions[i];
		attribs.color[i] = cubeVertices.colors[i];
	}

	std::vector<TriangleIndex> indices;
	indices.resize(36);
	for (int i = 0; i < 36; i++)
	{
		indices[i] = cubeIndices[i];
	}

	Cube.Create(attribs, indices);

	// Setup the instance transform attributes.
	GL(glBindVertexArray(Cube.vertexArrayObject));
	GL(glGenBuffers(1, &InstanceTransformBuffer));
	GL(glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformBuffer));
	GL(glBufferData(
			GL_ARRAY_BUFFER, NUM_INSTANCES * 4 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW));
	for (int i = 0; i < 4; i++)
	{
		GL(glEnableVertexAttribArray(VertexTransformAttribute + i));
		GL(glVertexAttribPointer(
				VertexTransformAttribute + i,
				4,
				GL_FLOAT,
				false,
				4 * 4 * sizeof(float),
				(void *) (i * 4 * sizeof(float))));
		GL(glVertexAttribDivisor(VertexTransformAttribute + i, 1));
	}
	GL(glBindVertexArray(0));

	// Setup random rotations.
	for (int i = 0; i < NUM_ROTATIONS; i++)
	{
		Rotations[i].x = RandomFloat();
		Rotations[i].y = RandomFloat();
		Rotations[i].z = RandomFloat();
	}

	// Setup random cube positions and rotations.
	for (int i = 0; i < NUM_INSTANCES; i++)
	{
		volatile float rx, ry, rz;
		for (;;)
		{
			rx = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));
			ry = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));
			rz = (RandomFloat() - 0.5f) * (50.0f + static_cast<float>(sqrt(NUM_INSTANCES)));

			// If too close to 0,0,0
			if (fabsf(rx) < 4.0f && fabsf(ry) < 4.0f && fabsf(rz) < 4.0f)
			{
				continue;
			}

			// Test for overlap with any of the existing cubes.
			bool overlap = false;
			for (int j = 0; j < i; j++)
			{
				if (fabsf(rx - CubePositions[j].x) < 4.0f &&
					fabsf(ry - CubePositions[j].y) < 4.0f &&
					fabsf(rz - CubePositions[j].z) < 4.0f)
				{
					overlap = true;
					break;
				}
			}

			if (!overlap)
			{
				break;
			}
		}

		rx *= 0.1f;
		ry *= 0.1f;
		rz *= 0.1f;

		// Insert into list sorted based on distance.
		int insert = 0;
		const float distSqr = rx * rx + ry * ry + rz * rz;
		for (int j = i; j > 0; j--)
		{
			const ovrVector3f *otherPos = &CubePositions[j - 1];
			const float otherDistSqr =
					otherPos->x * otherPos->x + otherPos->y * otherPos->y
					+ otherPos->z * otherPos->z;
			if (distSqr > otherDistSqr)
			{
				insert = j;
				break;
			}
			CubePositions[j] = CubePositions[j - 1];
			CubeRotations[j] = CubeRotations[j - 1];
		}

		CubePositions[insert].x = rx;
		CubePositions[insert].y = ry;
		CubePositions[insert].z = rz;

		CubeRotations[insert] = (int) (RandomFloat() * (NUM_ROTATIONS - 0.1f));
	}

	// Create SurfaceDef
	SurfaceDef.surfaceName = "Application Framework";
	SurfaceDef.graphicsCommand.Program = Program;
	SurfaceDef.graphicsCommand.GpuState.blendEnable = ovrGpuState::BLEND_ENABLE;
	SurfaceDef.graphicsCommand.GpuState.cullEnable = true;
	SurfaceDef.graphicsCommand.GpuState.depthEnable = true;
	SurfaceDef.geo = Cube;
	SurfaceDef.numInstances = NUM_INSTANCES;

	SurfaceRender.Init();

	startTime = GetTimeInSeconds();

	EnteredVrMode();
	return true;
}

void Application::EnteredVrMode()
{
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

	/// Get JNI
	const ovrJava *java = (reinterpret_cast<const ovrJava*>(GetContext()->ContextForVrApi()));
	TempJniEnv env(java->Vm);
	mSoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

	//mLocale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");
	//std::string fontName;
	//GetLocale().GetString("@string/font_name", "efigs.fnt", fontName);

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

	//if (num_refresh_rates > 0)
	//	vrapi_SetDisplayRefreshRate(app->GetOvrMobile(), mRefreshRates[num_refresh_rates - 1]);

	// Bind the delegates.

	controllers.SetToggleTexturesDelegate(
			std::bind(&ClientRenderer::ToggleTextures, &clientRenderer));
	controllers.SetToggleShowInfoDelegate(
			std::bind(&ClientRenderer::ToggleShowInfo, &clientRenderer));
	controllers.SetSetStickOffsetDelegate(
			std::bind(&ClientRenderer::SetStickOffset, &clientRenderer, std::placeholders::_1,
					  std::placeholders::_2));

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

	Vector3f currentRotation;
	currentRotation.x = (float) (vrFrame.PredictedDisplayTime - startTime);
	currentRotation.y = (float) (vrFrame.PredictedDisplayTime - startTime);
	currentRotation.z = (float) (vrFrame.PredictedDisplayTime - startTime);

	ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
	for (int i = 0; i < NUM_ROTATIONS; i++)
	{
		rotationMatrices[i] = ovrMatrix4f_CreateRotation(
				Rotations[i].x * currentRotation.x,
				Rotations[i].y * currentRotation.y,
				Rotations[i].z * currentRotation.z);
	}

	// Update the instance transform attributes.
	GL(glBindBuffer(GL_ARRAY_BUFFER, InstanceTransformBuffer));
	GL(Matrix4f *cubeTransforms = (Matrix4f *) glMapBufferRange(
			GL_ARRAY_BUFFER,
			0,
			NUM_INSTANCES * sizeof(Matrix4f),
			   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
	for (int i = 0; i < NUM_INSTANCES; i++)
	{
		const int index = CubeRotations[i];

		// Write in order in case the mapped buffer lives on write-combined memory.
		cubeTransforms[i].M[0][0] = rotationMatrices[index].M[0][0];
		cubeTransforms[i].M[0][1] = rotationMatrices[index].M[0][1];
		cubeTransforms[i].M[0][2] = rotationMatrices[index].M[0][2];
		cubeTransforms[i].M[0][3] = rotationMatrices[index].M[0][3];

		cubeTransforms[i].M[1][0] = rotationMatrices[index].M[1][0];
		cubeTransforms[i].M[1][1] = rotationMatrices[index].M[1][1];
		cubeTransforms[i].M[1][2] = rotationMatrices[index].M[1][2];
		cubeTransforms[i].M[1][3] = rotationMatrices[index].M[1][3];

		cubeTransforms[i].M[2][0] = rotationMatrices[index].M[2][0];
		cubeTransforms[i].M[2][1] = rotationMatrices[index].M[2][1];
		cubeTransforms[i].M[2][2] = rotationMatrices[index].M[2][2];
		cubeTransforms[i].M[2][3] = rotationMatrices[index].M[2][3];

		cubeTransforms[i].M[3][0] = CubePositions[i].x;
		cubeTransforms[i].M[3][1] = CubePositions[i].y;
		cubeTransforms[i].M[3][2] = CubePositions[i].z;
		cubeTransforms[i].M[3][3] = 1.0f;
	}
	GL(glUnmapBuffer(GL_ARRAY_BUFFER));
	GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

	CenterEyeViewMatrix = OVR::Matrix4f(vrFrame.HeadPose);
	Frame(vrFrame);

	// Update GUI systems last, but before rendering anything.
	mGuiSys->Frame(vrFrame, ovrMatrix4f_CreateIdentity());
	return OVRFW::ovrApplFrameOut();
}

OVRFW::ovrApplFrameOut Application::Frame(const OVRFW::ovrApplFrameIn& vrFrame)
{
	// we don't want local slide movements.
	mScene.SetMoveSpeed(1.0f);
	mScene.Frame(vrFrame,-1,false);
	clientRenderer.eyeSeparation=vrFrame.IPD;
	GLCheckErrorsWithTitle("Frame: Start");
	// Try to find remote controller
	if((int)controllers.mControllerIDs[0] == 0)
	{
		controllers.InitializeController(GetSessionObject());
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
			out.FrameMatrices.CenterView = CenterEyeViewMatrix;
			for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
			{
				out.FrameMatrices.EyeView[eye] = in.Eye[eye].ViewMatrix;
				// Calculate projection matrix using custom near plane value.
				out.FrameMatrices.EyeProjection[eye] = ovrMatrix4f_CreateProjectionFov(
						SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, 0.0f, 0.0f, 0.1f,
						0.0f);
			}

			/// Surface
			//out.Surfaces.push_back(ovrDrawSurface(&SurfaceDef));

			// Append mGuiSys surfaces.
			//mGuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);

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
//Build frame
	mScene.GetFrameMatrices(SuggestedEyeFovDegreesX, SuggestedEyeFovDegreesY, out.FrameMatrices);
	mScene.GenerateFrameSurfaceList(out.FrameMatrices, out.Surfaces);

// The camera should be where our head is. But when rendering, the camera is in OVR space, so:
	GlobalGraphicsResources.scrCamera->UpdatePosition(clientDeviceState.headPose.position);

	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	//out.FrameIndex = vrFrame.FrameNumber;
	//out.DisplayTime = vrFrame.PredictedDisplayTimeInSeconds;
	//out.SwapInterval = app->GetSwapInterval();

	//out.FrameFlags = 0;
	//out.LayerCount = 0;

/*	ovrLayerProjection2 &worldLayer = out.Layers[out.LayerCount++].Projection;

	worldLayer = vrapi_DefaultLayerProjection2();
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
	worldLayer.HeadPose = vrFrame.Tracking.HeadPose;
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		worldLayer.Textures[eye].ColorSwapChain = vrFrame.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = vrFrame.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
	}*/

	GLCheckErrorsWithTitle("Frame: Pre-Cubemap");
	clientRenderer.CopyToCubemaps(mDeviceContext);
// Append video surface
	clientRenderer.RenderVideo(mDeviceContext, out);

	if (sessionClient.IsConnected())
	{
		clientRenderer.Render(in, mGuiSys);
	}
	else
	{
		//out.ClearColorBuffer = true;
		//out.ClearDepthBuffer = true;
		lobbyRenderer.Render(mGuiSys);
	};

//Append SCR Nodes to surfaces.
	GLCheckErrorsWithTitle("Frame: Pre-SCR");
	uint32_t time_elapsed = (uint32_t) (in.DeltaSeconds * 1000.0f);
	resourceManagers.Update(time_elapsed);
	resourceCreator.Update(time_elapsed);

//Move the hands before they are drawn.
	UpdateHandObjects();
	clientRenderer.RenderLocalNodes(out);
	GLCheckErrorsWithTitle("Frame: Post-SCR");

// Append GuiSys surfaces. This should always be the last item to append the render list.
	mGuiSys->AppendSurfaceList(out.FrameMatrices.CenterView, &out.Surfaces);
/*	if(useMultiview)
	{
		// Initialize the FrameParms.
		FrameParms = vrapi_DefaultFrameParms( app->GetJava(), VRAPI_FRAME_INIT_DEFAULT, vrapi_GetTimeInSeconds(), NULL );
		for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
		{
			out.Layers[0].Textures[eye].ColorTextureSwapChain = vrFrame.ColorTextureSwapChain[eye];
			//FrameParms.Layers[0].Textures[eye].DepthTextureSwapChain = vrFrame.DepthTextureSwapChain[eye];
			out.Layers[0].Textures[eye].TextureSwapChainIndex = vrFrame.TextureSwapChainIndex;

			out.Layers[0].Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
			out.Layers[0].Textures[eye].HeadPose = vrFrame.Tracking.HeadPose;
		}

		//FrameParms.ExternalVelocity = mScene.GetExternalVelocity();
		out.Layers[0].Flags = VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	}*/
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

void Application::AppRenderEye(
		const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out, int eye)
{
	// Render the surfaces returned by Frame.
	SurfaceRender.RenderSurfaceList(
			out.Surfaces,
			out.FrameMatrices.EyeView[0], // always use 0 as it assumes an array
			out.FrameMatrices.EyeProjection[0], // always use 0 as it assumes an array
			eye);
}

void Application::OnVideoStreamChanged(const char *server_ip, const avs::SetupCommand &setupCommand
									   , avs::Handshake &handshake)
{
	const avs::VideoConfig &videoConfig = setupCommand.video_config;
	if (!mPipelineConfigured)
	{
		OVR_WARN("VIDEO STREAM CHANGED: %d %d %d, cubemap %d", setupCommand.port,
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
								setupCommand.port + 1, server_ip, setupCommand.port
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
	if (GlobalGraphicsResources.defaultPBREffect.HasEffectPass(effectPassName))
	{
		return GlobalGraphicsResources.defaultPBREffect.GetEffectPassCreateInfo(effectPassName);
	}

	scr::ShaderSystem::PassVariables pv;
	pv.mask = false;
	pv.reverseDepth = false;
	pv.msaa = false;

	scr::ShaderSystem::Pipeline gp(&GlobalGraphicsResources.renderPlatform, pipelineCreateInfo);

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

	GlobalGraphicsResources.defaultPBREffect.CreatePass(&ci);
	GlobalGraphicsResources.defaultPBREffect.LinkShaders(effectPassName, shaderResources);

	return GlobalGraphicsResources.defaultPBREffect.GetEffectPassCreateInfo(effectPassName);
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
