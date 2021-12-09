#pragma once

#include "Appl.h"
#include "System.h"
#include "GUI/GuiSys.h"
#include "Locale/OVR_Locale.h"
#include "Misc/Log.h"
//#include "Input/TinyUI.h"
#include <memory>
#include <vector>
#include <string>
#include <SoundEffectContext.h>
#include <Model/SceneView.h>

#include <libavstream/libavstream.hpp>
#include <libavstream/geometrydecoder.hpp>
#include <libavstream/audiodecoder.h>
#include <libavstream/audio/audiotarget.h>

#include "crossplatform/GeometryDecoder.h"
#include "crossplatform/ResourceCreator.h"
#include "crossplatform/SessionClient.h"
#include "crossplatform/AudioStreamTarget.h"
#include "crossplatform/AudioPlayer.h"
#include "crossplatform/NetworkPipeline.h"

#include "GlobalGraphicsResources.h"
#include "VideoDecoderProxy.h"
#include "LobbyRenderer.h"
#include "ClientRenderer.h"
#include "UIRenderer.h"
#include "ClientDeviceState.h"

#include "SCR_Class_Android_Impl/Android_MemoryUtil.h"

using OVR::Axis_X;
using OVR::Axis_Y;
using OVR::Axis_Z;
using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3d;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW
{

	static const int CPU_LEVEL = 2;
	static const int GPU_LEVEL = 3;
	static const int NUM_INSTANCES = 1500;
	static const int NUM_ROTATIONS = 16;
	static const Vector4f CLEAR_COLOR(0.125f, 0.0f, 0.125f, 1.0f);

	class Application : public ovrAppl, public SessionCommandInterface, public DecodeEventInterface, public ClientAppInterface
	{
	public:
		Application();
		virtual ~Application();

		// Called when the application initializes.
		// Must return true if the application initializes successfully.
		virtual bool AppInit(const OVRFW::ovrAppContext* context) override;
		// Called when the application shuts down
		virtual void AppShutdown(const OVRFW::ovrAppContext* context) override;
		// Called when the application is resumed by the system.
		virtual void AppResumed(const OVRFW::ovrAppContext* contet) override;
		// Called when the application is paused by the system.
		virtual void AppPaused(const OVRFW::ovrAppContext* context) override;
		// Called once per frame when the VR session is active.
		virtual OVRFW::ovrApplFrameOut AppFrame(const OVRFW::ovrApplFrameIn& in) override;
		// Called once per frame to allow the application to render eye buffers.
		virtual void AppRenderFrame(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override;
		// Called once per eye each frame for default renderer
		virtual void AppRenderEye(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out, int eye) override;

		/* Begin SessionCommandInterface */
		virtual bool OnSetupCommandReceived(const char* server_ip, const avs::SetupCommand& setupCommand, avs::Handshake& handshake) override;
		virtual void OnVideoStreamClosed() override;
		virtual void OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand) override;
		virtual bool OnNodeEnteredBounds(avs::uid id) override;
		virtual bool OnNodeLeftBounds(avs::uid id) override;
		virtual void OnLightingSetupChanged(const avs::SetupLightingCommand &) override;
		void UpdateNodeStructure(const avs::UpdateNodeStructureCommand& updateNodeStructureCommand) override;
		virtual std::vector<avs::uid> GetGeometryResources() override;
		virtual void ClearGeometryResources() override;
		virtual void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) override;

		virtual void UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList) override;
		virtual void UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList) override;
		virtual void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
		virtual void UpdateNodeAnimation(const avs::ApplyAnimation& animationUpdate) override;
		virtual void UpdateNodeAnimationControl(const avs::NodeUpdateAnimationControl& animationControlUpdate) override;
		virtual void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) override;
		/* End SessionCommandInterface */

		/* Begin DecodeEventInterface */
		virtual void OnFrameAvailable() override;
		/* End DecodeEventInterface */

		// UI to connect.
		void ConnectButtonHandler(const std::string &url);
	private:
		OVRFW::ovrApplFrameOut Frame(const OVRFW::ovrApplFrameIn& vrFrame);
		void DrawConnectionStateOSD(OVRFW::OvrGuiSys *mGuiSys,OVRFW::ovrRendererOutput &out);
		void Render(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out);
		void UpdateHandObjects();
		bool ProcessIniFile();
		void EnteredVrMode();
		void ToggleMenu();
		ovrRenderState RenderState;
		ovrFileSys* FileSys;
		ovrLocale* Locale;
		ovrSurfaceRender SurfaceRender;
		ovrSurfaceDef SurfaceDef;
		unsigned int Random;

		double startTime;

		float RandomFloat();

		/////////////////////

		static void avsMessageHandler(avs::LogSeverity severity, const char *msg, void *);
		const scr::Effect::EffectPassCreateInfo *BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl, const scr::ShaderSystem::PipelineCreateInfo*, const std::vector<scr::ShaderResource>& shaderResources) override;
		void DrawTexture(avs::vec3 &offset,scr::Texture &texture) override;
		void PrintText(avs::vec3 &offset,avs::vec4 &colour,const char *txt,...) override;
		std::string LoadTextFile(const char *filename) override;


		avs::Context  mContext;
		avs::Pipeline mPipeline;

		avs::Surface       mSurface;
		bool               mPipelineConfigured;

		static constexpr size_t NumVideoStreams = 1;
		static constexpr bool AudioStream = true;
		static constexpr bool   GeoStream  = true;

		GeometryDecoder        geometryDecoder;
		avs::GeometryDecoder   avsGeometryDecoder;
		avs::GeometryTarget    avsGeometryTarget;

		avs::AudioDecoder avsAudioDecoder;
		avs::AudioTarget avsAudioTarget;
		std::unique_ptr<sca::AudioStreamTarget> audioStreamTarget;
		sca::AudioPlayer* audioPlayer;
		std::unique_ptr<sca::NetworkPipeline> mNetworkPipeline;
		avs::Queue mAudioInputQueue;

		OVRFW::OvrGuiSys::SoundEffectPlayer *mSoundEffectPlayer;

		OVRFW::OvrGuiSys *mGuiSys;
		OVRFW::VRMenu* menu= nullptr;
		teleport::TinyUI tinyUI;

		OVRFW::OvrSceneView mScene;

		SessionClient                       sessionClient;

		std::vector<float> mRefreshRates;

		int mNumPendingFrames                  = 0;

		//Clientside Renderering Objects
		std::unique_ptr<scc::GL_DeviceContext> mDeviceContext;

		unsigned long long receivedInitialPos=0;
		unsigned long long receivedRelativePos=0;

		ClientRenderer clientRenderer;
		LobbyRenderer lobbyRenderer;
		//teleport::UIRenderer uIRenderer;
		std::unique_ptr<Android_MemoryUtil> memoryUtil;
		scr::GeometryCache geometryCache;
		scr::ResourceCreator resourceCreator;
		Controllers controllers;

		std::string server_ip;
		int server_discovery_port=10600;
		int client_service_port=10501;
		int client_streaming_port=10502;

		ENetAddress remoteEndpoint;
		teleport::client::ClientDeviceState clientDeviceState;

		avs::vec3 bodyOffsetFromHead; //Offset of player body from head pose.

		OVR::Matrix4f lastCenterView;

		float frameRate = 1.0f;
		ovrDeviceType DeviceType=VRAPI_DEVICE_TYPE_UNKNOWN;
		bool should_connect=false;
	};

}