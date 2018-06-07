// (C) Copyright 2018 Simul.co

#include "Application.h"
#include "Config.h"
#include "Input.h"

#include "GuiSys.h"
#include "OVR_Locale.h"

#include <enet/enet.h>

#if defined( OVR_OS_WIN32 )
#include "../res_pc/resource.h"
#endif

using namespace OVR;

#if defined( OVR_OS_ANDROID )
extern "C" {

jlong Java_co_Simul_remoteplayclient_MainActivity_nativeSetAppInterface(JNIEnv* jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	LOG("nativeSetAppInterface");
	Application::InitializeJNI(jni);
	return (new Application())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

void Java_co_Simul_remoteplayclient_MainActivity_nativeFrameAvailable(JNIEnv* jni, jclass clazz, jlong interfacePtr)
{
	Application* app = static_cast<Application*>(reinterpret_cast<OVR::App*>(interfacePtr)->GetAppInterface());
	app->NotifyFrameAvailable();
}

} // extern "C"

#endif

namespace shaders {

static const char* VideoSurface_OPTIONS = R"(
	#extension GL_OES_EGL_image_external : enable
	#extension GL_OES_EGL_image_external_essl3 : enable
)";

static const char* VideoSurface_VS = R"(
    attribute vec4 position;
    varying highp vec3 vSampleVec;

    void main() {
		// Equirect map sampling vector is rotated -90deg on Y axis to match UE4 yaw.
		vSampleVec  = normalize(vec3(-position.z, position.y, position.x));
        gl_Position = TransformVertex(position);
    }
)";

static const char* VideoSurface_FS = R"(
	const highp float PI    = 3.141592;
	const highp float TwoPI = 2.0 * PI;

    uniform samplerExternalOES videoFrameTexture;
	varying highp vec3 vSampleVec;

	void main() {
		highp float phi   = atan(vSampleVec.z, vSampleVec.x) / TwoPI;
		highp float theta = acos(vSampleVec.y) / PI;
		highp vec2  uv    = fract(vec2(phi, theta));
		gl_FragColor = texture2D(videoFrameTexture, uv);
	}
)";

} // shaders

Application::JNI Application::jni = {};

Application::Application()
	: mSoundEffectContext(nullptr)
	, mSoundEffectPlayer(nullptr)
	, mGuiSys(OvrGuiSys::Create())
	, mLocale(nullptr)
	, mVideoSurfaceTexture(nullptr)
    , mSession(this)
	, mControllerID(-1)
{
	if(enet_initialize() != 0) {
		FAIL("Failed to initialize ENET library");
	}
}

Application::~Application()
{
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

	settings.TrackingTransform = VRAPI_TRACKING_TRANSFORM_SYSTEM_CENTER_EYE_LEVEL;
	settings.RenderMode = RENDERMODE_MONO;
}

void Application::EnteredVrMode(const ovrIntentType intentType, const char* intentFromPackage, const char* intentJSON, const char* intentURI )
{
	OVR_UNUSED(intentFromPackage);
	OVR_UNUSED(intentJSON);
	OVR_UNUSED(intentURI);

	if(intentType == INTENT_LAUNCH)
	{
		const ovrJava* java = app->GetJava();

		InitializeController();

		mSoundEffectContext = new ovrSoundEffectContext(*java->Env, java->ActivityObject);
		mSoundEffectContext->Initialize(&app->GetFileSys());
		mSoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

		mLocale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");

		String fontName;
		GetLocale().GetString("@string/font_name", "efigs.fnt", fontName);
		mGuiSys->Init(this->app, *mSoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines());

		{
			const ovrProgramParm uniformParms[] = {
					{ "videoFrameTexture", ovrProgramParmType::TEXTURE_SAMPLED },
			};
			mVideoSurfaceProgram = GlProgram::Build(nullptr, shaders::VideoSurface_VS,
													shaders::VideoSurface_OPTIONS, shaders::VideoSurface_FS,
													uniformParms, 1);

			if(!mVideoSurfaceProgram.IsValid()) {
				FAIL("Failed to build video surface shader program");
				return;
			}
		}

		mVideoSurfaceTexture = new SurfaceTexture(app->GetJava()->Env);
        mVideoTexture = GlTexture(mVideoSurfaceTexture->GetTextureId(), GL_TEXTURE_EXTERNAL_OES, 0, 0);

		mVideoSurfaceDef.surfaceName = "VideoSurface";
		mVideoSurfaceDef.geo = BuildGlobe();
		mVideoSurfaceDef.graphicsCommand.Program = mVideoSurfaceProgram;
		mVideoSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
		mVideoSurfaceDef.graphicsCommand.GpuState.cullEnable = false;
        mVideoSurfaceDef.graphicsCommand.UniformData[0].Data = &mVideoTexture;

		mSession.Connect(REMOTEPLAY_SERVER_IP, REMOTEPLAY_SERVER_PORT, REMOTEPLAY_TIMEOUT);
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
			app->ShowSystemUI(VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
			continue;
		}
	}

    // Query controller input state.
    ControllerState controllerState = {};
    if(mControllerID != -1) {
		ovrInputStateTrackedRemote ovrState;
		ovrState.Header.ControllerType = ovrControllerType_TrackedRemote;
		if(vrapi_GetCurrentInputState(app->GetOvrMobile(), mControllerID, &ovrState.Header) >= 0) {
			controllerState.mButtons = ovrState.Buttons;
			controllerState.mTrackpadStatus = ovrState.TrackpadStatus > 0;
			controllerState.mTrackpadX = ovrState.TrackpadPosition.x / mTrackpadDim.x;
			controllerState.mTrackpadY = ovrState.TrackpadPosition.y / mTrackpadDim.y;
		}
    }

	// Handle networked session.
    mSession.Frame(vrFrame, controllerState);

	// Update video texture if we have any pending decoded frames.
	while(mNumPendingFrames > 0) {
		mVideoSurfaceTexture->Update();
		--mNumPendingFrames;
	}

	ovrFrameResult res;

	mScene.Frame(vrFrame);
	mScene.GetFrameMatrices(vrFrame.FovX, vrFrame.FovY, res.FrameMatrices);
	mScene.GenerateFrameSurfaceList(res.FrameMatrices, res.Surfaces);

	// Update GUI systems after the app frame, but before rendering anything.
	mGuiSys->Frame(vrFrame, res.FrameMatrices.CenterView);

	res.FrameIndex   = vrFrame.FrameNumber;
	res.DisplayTime  = vrFrame.PredictedDisplayTimeInSeconds;
	res.SwapInterval = app->GetSwapInterval();

	res.FrameFlags = 0;
	res.LayerCount = 0;

	ovrLayerProjection2& worldLayer = res.Layers[res.LayerCount++].Projection;

	worldLayer = vrapi_DefaultLayerProjection2();
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
	worldLayer.HeadPose = vrFrame.Tracking.HeadPose;
	for(int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
		worldLayer.Textures[eye].ColorSwapChain = vrFrame.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = vrFrame.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
	}

	// Append video surface
	res.Surfaces.PushBack(ovrDrawSurface(&mVideoSurfaceDef));

	// Append GuiSys surfaces.
	mGuiSys->AppendSurfaceList(res.FrameMatrices.CenterView, &res.Surfaces);

	GL_CheckErrors("Frame");
	return res;
}

void Application::InitializeJNI(JNIEnv* env)
{
	jni.activityClass = env->FindClass("co/Simul/remoteplayclient/MainActivity");
	jni.initializeVideoStreamMethod = env->GetMethodID(jni.activityClass, "initializeVideoStream", "(IIILandroid/graphics/SurfaceTexture;)V");
    jni.closeVideoStreamMethod = env->GetMethodID(jni.activityClass, "closeVideoStream", "()V");
}

bool Application::InitializeController()
{
	ovrInputCapabilityHeader inputCapsHeader;
	for(uint32_t i = 0;
		vrapi_EnumerateInputDevices(app->GetOvrMobile(), i, &inputCapsHeader) == 0; ++i) {
		if(inputCapsHeader.Type == ovrControllerType_TrackedRemote) {
			mControllerID = inputCapsHeader.DeviceID;
			break;
		}
	}

	if(mControllerID != -1) {
		LOG("Using controller ID: %x", mControllerID);

		ovrInputTrackedRemoteCapabilities trackedInputCaps;
		trackedInputCaps.Header = inputCapsHeader;
		vrapi_GetInputDeviceCapabilities(app->GetOvrMobile(), &trackedInputCaps.Header);
		mTrackpadDim.x = trackedInputCaps.TrackpadMaxX;
		mTrackpadDim.y = trackedInputCaps.TrackpadMaxY;
		return true;
	}
	else {
		WARN("No suitable remote controller found. Disabling input!");
		return false;
	}
}

void Application::OnVideoStreamChanged(uint port, uint width, uint height)
{
    WARN("VIDEO STREAM CHANGED: %d %d %d", port, width, height);

	const ovrJava* java = app->GetJava();
	java->Env->CallVoidMethod(java->ActivityObject, jni.initializeVideoStreamMethod, port, width, height, mVideoSurfaceTexture->GetJavaObject());
}

void Application::OnVideoStreamClosed()
{
    WARN("VIDEO STREAM CLOSED");

	const ovrJava* java = app->GetJava();
	java->Env->CallVoidMethod(java->ActivityObject, jni.closeVideoStreamMethod);
}

