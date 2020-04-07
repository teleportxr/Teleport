// (C) Copyright 2018 Simul.co

#pragma once

#include <VrApi_Types.h>
#include <VrApi_Input.h>

#include "App.h"
#include "GuiSys.h"
#include "SceneView.h"
#include "SoundEffectContext.h"

#include <libavstream/libavstream.hpp>
#include <libavstream/geometrydecoder.hpp>

#include "crossplatform/GeometryDecoder.h"
#include "crossplatform/ResourceCreator.h"
#include "crossplatform/SessionClient.h"

#include "GlobalGraphicsResources.h"
#include "VideoDecoderProxy.h"
#include "LobbyRenderer.h"
#include "ClientRenderer.h"

namespace OVR {
	class ovrLocale;
}

namespace scr {
	class Texture;
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
	virtual void OnVideoStreamChanged(const avs::SetupCommand &setupCommand, avs::Handshake& handshake, bool shouldClearEverything, std::vector<avs::uid>& resourcesClientNeeds, std::vector<avs::uid>& outExistingActors) override;

	virtual void OnVideoStreamClosed() override;

	virtual bool OnActorEnteredBounds(avs::uid actor_uid) override;

	virtual bool OnActorLeftBounds(avs::uid actor_uid) override;
	/* End SessionCommandInterface */

	/* Begin DecodeEventInterface */
	virtual void OnFrameAvailable() override;
	/* End DecodeEventInterface */

private:
	avs::HeadPose headPose;
	avs::HeadPose controllerPoses[2];
	void UpdateHandObjects();
	struct CubemapUB
	{
		scr::ivec2 sourceOffset;
		uint32_t   faceSize;
		uint32_t    mip = 0;
		uint32_t    face = 0;
	};
	CubemapUB cubemapUB;
	struct VideoUB
	{
		scr::vec4 eyeOffsets[2];
		ovrMatrix4f invViewProj[2];
		scr::vec3 cameraPosition;
		int pad_;
	};
	VideoUB videoUB;

	static void avsMessageHandler(avs::LogSeverity severity, const char *msg, void *);

	avs::Context  mContext;
	avs::Pipeline mPipeline;

	avs::Decoder       mDecoder;
	avs::Surface       mSurface;
	avs::NetworkSource mNetworkSource;
	bool               mPipelineConfigured;

	static constexpr size_t NumStreams = 1;
	static constexpr bool   GeoStream  = true;

	GeometryDecoder        geometryDecoder;
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
	OVR::SurfaceTexture *mVideoSurfaceTexture;
	std::shared_ptr<scr::Texture>       mVideoTexture;
	std::shared_ptr<scr::Texture>       mCubemapTexture;
	std::shared_ptr<scr::Texture>       mDiffuseTexture;
	std::shared_ptr<scr::Texture>       mSpecularTexture;
	std::shared_ptr<scr::Texture>       mRoughSpecularTexture;
	std::shared_ptr<scr::Texture>       mCubemapLightingTexture;
	std::shared_ptr<scr::UniformBuffer> mCubemapUB;
	std::shared_ptr<scr::UniformBuffer> mVideoUB;
	std::shared_ptr<scr::ShaderStorageBuffer> mCameraPositionBuffer;
	std::vector<scr::ShaderResource>    mCubemapComputeShaderResources;
	std::shared_ptr<scr::Effect>        mCopyCubemapEffect;
	std::shared_ptr<scr::Effect>        mCopyCubemapWithDepthEffect;
	std::shared_ptr<scr::Effect>        mExtractCameraPositionEffect;

	GlobalGraphicsResources& GlobalGraphicsResources = GlobalGraphicsResources::GetInstance();

	std::string                         CopyCubemapSrc;
	std::string                         ExtractPositionSrc;

	ovrMobile                           *mOvrMobile;
	SessionClient                       mSession;

	std::vector<float> mRefreshRates;

	ovrDeviceID mControllerID;

	ovrVector2f mTrackpadDim;
    ControllerState mLastPrimaryControllerState; //State of the primary controller on the last frame.

	const scr::quat HAND_ROTATION_DIFFERENCE {0.0000000456194194, 0.923879385, -0.382683367, 0.000000110135019}; //Adjustment to the controller's rotation to get the desired rotation.

	int mNumPendingFrames                  = 0;

	//Clientside Renderering Objects
	scc::GL_DeviceContext mDeviceContext;

	scr::vec4 mCameraPositions[8];
	scr::vec3 cameraPosition;	// in real space.

	bool receivedInitialPos=false;

	void CopyToCubemaps();
    const scr::Effect::EffectPassCreateInfo& BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl, const scr::ShaderSystem::PipelineCreateInfo*, const std::vector<scr::ShaderResource>& shaderResources);
	std::string LoadTextFile(const char *filename);
	bool mShowInfo=true;
	ClientRenderer clientRenderer;
	scr::ResourceManagers resourceManagers;
	ResourceCreator        resourceCreator;
	LobbyRenderer lobbyRenderer;
};
