// (C) Copyright 2018 Simul.co

#pragma once

#include "App.h"
#include "SceneView.h"
#include "SoundEffectContext.h"
#include "GuiSys.h"

namespace OVR {
	class ovrLocale;
}

class Application : public OVR::VrAppInterface
{
public:
	Application();
	virtual	~Application();

	virtual void Configure( OVR::ovrSettings & settings );
	virtual void EnteredVrMode( const OVR::ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI );
	virtual void LeavingVrMode();
	virtual bool OnKeyEvent( const int keyCode, const int repeatCount, const OVR::KeyEventType eventType );
	virtual OVR::ovrFrameResult Frame( const OVR::ovrFrameInput & vrFrame );

	class OVR::ovrLocale& GetLocale() { return *Locale; }
	void NotifyFrameAvailable() { ++mNumPendingFrames; }

	static void InitializeJNI(JNIEnv* env);

private:
	OVR::ovrSoundEffectContext* SoundEffectContext;
	OVR::OvrGuiSys::SoundEffectPlayer* SoundEffectPlayer;
	OVR::OvrGuiSys* GuiSys;
	OVR::ovrLocale* Locale;

	OVR::OvrSceneView Scene;

    OVR::ovrSurfaceDef mVideoSurfaceDef;
	OVR::GlProgram mVideoSurfaceProgram;
    OVR::SurfaceTexture* mVideoSurfaceTexture;
	OVR::GlTexture mVideoTexture;

	int mNumPendingFrames = 0;

	struct JNI {
		JNIEnv* env;
		jclass activityClass;
		jmethodID initializeVideoStreamMethod;
	};
	static JNI jni;

	static const int VideoWidth  = 2048;
	static const int VideoHeight = 1024;
};
