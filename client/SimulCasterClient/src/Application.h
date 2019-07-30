// (C) Copyright 2018 Simul.co

#pragma once

#include <VrApi_Types.h>
#include <VrApi_Input.h>

#include <libavstream/libavstream.hpp>

#include "App.h"
#include "SceneView.h"
#include "SoundEffectContext.h"
#include "GuiSys.h"

#include "SessionClient.h"
#include "VideoDecoderProxy.h"

#include "MeshCreator.h"
#include "GeometryDecoder.h"
#include <libavstream/geometrydecoder.hpp>

namespace OVR {
	class ovrLocale;
}

class Application : public OVR::VrAppInterface, public SessionCommandInterface, public DecodeEventInterface
{
public:
	Application();
	virtual	~Application();

	virtual void Configure(OVR::ovrSettings& settings) override;
	virtual void EnteredVrMode(const OVR::ovrIntentType intentType, const char* intentFromPackage, const char* intentJSON, const char* intentURI) override;
	virtual void LeavingVrMode() override;
	virtual bool OnKeyEvent(const int keyCode, const int repeatCount, const OVR::KeyEventType eventType);
	virtual OVR::ovrFrameResult Frame(const OVR::ovrFrameInput& vrFrame) override;

	class OVR::ovrLocale& GetLocale() { return *mLocale; }

	bool InitializeController();

	/* Begin SessionCommandInterface */
	virtual void OnVideoStreamChanged(uint port, uint width, uint height) override;
	virtual void OnVideoStreamClosed() override;
	/* End SessionCommandInterface */

	/* Begin DecodeEventInterface */
	virtual void OnFrameAvailable() override;
	/* End DecodeEventInterface */

private:
	static void avsMessageHandler(avs::LogSeverity severity, const char* msg, void*);

	avs::Context mContext;
	avs::Pipeline mPipeline;

	avs::Decoder mDecoder;
	avs::Surface mSurface;
	avs::NetworkSource mNetworkSource;
    bool mPipelineConfigured;

    static constexpr size_t   NumStreams =1;
    static constexpr bool     GeoStream  =true;

    GeometryDecoder geometryDecoder;
	MeshCreator meshCreator;
	avs::GeometryDecoder avsGeometryDecoder;
	avs::GeometryTarget avsGeometryTarget;

	OVR::ovrSoundEffectContext* mSoundEffectContext;
	OVR::OvrGuiSys::SoundEffectPlayer* mSoundEffectPlayer;

	OVR::OvrGuiSys* mGuiSys;
	OVR::ovrLocale* mLocale;

	OVR::OvrSceneView mScene;

    OVR::ovrSurfaceDef mVideoSurfaceDef;
	OVR::GlProgram mVideoSurfaceProgram;
	OVR::GlTexture mVideoTexture;
	OVR::SurfaceTexture* mVideoSurfaceTexture;

	SessionClient mSession;

	ovrDeviceID mControllerID;
	ovrVector2f mTrackpadDim;

	int mNumPendingFrames = 0;
};
