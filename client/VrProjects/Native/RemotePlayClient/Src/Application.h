// (C) Copyright 2018 Simul.co

#pragma once

#include <VrApi_Types.h>
#include <VrApi_Input.h>

#include "App.h"
#include "SceneView.h"
#include "SoundEffectContext.h"
#include "GuiSys.h"

#include "SessionClient.h"

namespace OVR {
	class ovrLocale;
}

class Application : public OVR::VrAppInterface, public SessionCommandInterface
{
public:
	Application();
	virtual	~Application();

	virtual void Configure(OVR::ovrSettings& settings);
	virtual void EnteredVrMode(const OVR::ovrIntentType intentType, const char* intentFromPackage, const char* intentJSON, const char* intentURI);
	virtual void LeavingVrMode();
	virtual bool OnKeyEvent(const int keyCode, const int repeatCount, const OVR::KeyEventType eventType );
	virtual OVR::ovrFrameResult Frame(const OVR::ovrFrameInput& vrFrame);

	class OVR::ovrLocale& GetLocale() { return *mLocale; }
	void NotifyFrameAvailable() { ++mNumPendingFrames; }

	static void InitializeJNI(JNIEnv* env);
	bool InitializeController();

	/* Begin SessionCommandInterface */
	virtual void OnVideoStreamChanged(uint port, uint width, uint height) override;
	virtual void OnVideoStreamClosed() override;
	/* End SessionCommandInterface */

private:
	OVR::ovrSoundEffectContext* mSoundEffectContext;
	OVR::OvrGuiSys::SoundEffectPlayer* mSoundEffectPlayer;

	OVR::OvrGuiSys* mGuiSys;
	OVR::ovrLocale* mLocale;

	OVR::OvrSceneView mScene;

    OVR::ovrSurfaceDef mVideoSurfaceDef;
	OVR::GlProgram mVideoSurfaceProgram;
    OVR::SurfaceTexture* mVideoSurfaceTexture;
	OVR::GlTexture mVideoTexture;

	SessionClient mSession;

	ovrDeviceID mControllerID;
	ovrVector2f mTrackpadDim;

	int mNumPendingFrames = 0;

	struct JNI {
		JNIEnv* env;
		jclass activityClass;
		jmethodID initializeVideoStreamMethod;
		jmethodID closeVideoStreamMethod;
	};
	static JNI jni;
};
