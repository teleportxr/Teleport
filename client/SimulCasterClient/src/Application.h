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

#include "ResourceCreator.h"
#include "GeometryDecoder.h"
#include <libavstream/geometrydecoder.hpp>
#include "SCR_Class_GL_Impl/GL_RenderPlatform.h"

namespace OVR {
	class ovrLocale;
}

class Application : public OVR::VrAppInterface, public SessionCommandInterface, public DecodeEventInterface
{
public:
	Application();

	virtual    ~Application();

	virtual void Configure(OVR::ovrSettings &settings) override;

	virtual void EnteredVrMode(
			const OVR::ovrIntentType intentType, const char *intentFromPackage,
			const char *intentJSON, const char *intentURI) override;

	virtual void LeavingVrMode() override;

	virtual bool
	OnKeyEvent(const int keyCode, const int repeatCount, const OVR::KeyEventType eventType);

	virtual OVR::ovrFrameResult Frame(const OVR::ovrFrameInput &vrFrame) override;

	class OVR::ovrLocale &GetLocale()
	{
		return *mLocale;
	}

	bool InitializeController();

	/* Begin SessionCommandInterface */
	virtual void OnVideoStreamChanged(const avs::SetupCommand &setupCommand) override;

	virtual void OnVideoStreamClosed() override;
	/* End SessionCommandInterface */

	/* Begin DecodeEventInterface */
	virtual void OnFrameAvailable() override;
	/* End DecodeEventInterface */

private:
	static void avsMessageHandler(avs::LogSeverity severity, const char *msg, void *);

	avs::Context  mContext;
	avs::Pipeline mPipeline;

	avs::Decoder       mDecoder;
	avs::Surface       mSurface;
	avs::NetworkSource mNetworkSource;
	bool               mPipelineConfigured;

	static constexpr size_t NumStreams = 1;
	static constexpr bool   GeoStream  = true;

	scc::GL_RenderPlatform renderPlatform;
	GeometryDecoder        geometryDecoder;
	ResourceCreator        resourceCreator;
	avs::GeometryDecoder   avsGeometryDecoder;
	avs::GeometryTarget    avsGeometryTarget;

	struct RenderConstants
	{
		avs::vec4 colourOffsetScale;
		avs::vec4 depthOffsetScale;
	};
	RenderConstants        renderConstants;

	OVR::ovrSoundEffectContext        *mSoundEffectContext;
	OVR::OvrGuiSys::SoundEffectPlayer *mSoundEffectPlayer;

	OVR::OvrGuiSys *mGuiSys;
	OVR::ovrLocale *mLocale;

	OVR::OvrSceneView mScene;

	OVR::ovrSurfaceDef mVideoSurfaceDef;
	OVR::GlProgram     mVideoSurfaceProgram;
	OVR::GlTexture     mVideoTexture;
	OVR::SurfaceTexture *mVideoSurfaceTexture;
	ovrMobile     *mOvrMobile;
	SessionClient mSession;

	std::vector<float> mRefreshRates;

	ovrDeviceID mControllerID;
	//int         mControllerIndex;
	ovrVector2f mTrackpadDim;

	int mNumPendingFrames = 0;

	scr::ResourceManagers resourceManagers;
    //Clientside Renderering Objects
    scc::GL_DeviceContext mDeviceContext;
    scc::GL_Effect mFlatColourEffect;
    scc::GL_Texture mDummyTexture;
    std::shared_ptr<scr::Material> mFlatColourMaterial;
    std::map<avs::uid, OVR::ovrSurfaceDef> mOVRActors;
    inline void RemoveInvalidOVRActors()
	{
		for(std::map<avs::uid, OVR::ovrSurfaceDef>::iterator it = mOVRActors.begin(); it != mOVRActors.end(); it++)
		{
			if(resourceManagers.mActorManager.m_Actors.find(it->first) == resourceManagers.mActorManager.m_Actors.end())
			{
				mOVRActors.erase(it);
			}
		}
	}
};
