// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/camera_constants.sl"
#include "TeleportClient/basic_linear_algebra.h"
#include "ClientRender/GeometryCache.h"
#include "ClientRender/ResourceCreator.h"
#include <libavstream/src/platform.hpp>
#include "TeleportClient/SessionClient.h"
#include "ClientRender/GeometryDecoder.h"
#include "TeleportClient/ClientPipeline.h"
#include "TeleportClient/OpenXR.h"

namespace clientrender
{
	/// <summary>
	/// A 
	/// </summary>
	struct AVSTexture
	{
		virtual ~AVSTexture() = default;
		virtual avs::SurfaceBackendInterface* createSurface() const = 0;
	};
	using AVSTextureHandle = std::shared_ptr<AVSTexture>;

	struct RendererStats
	{
		uint64_t frameCounter;
		double lastFrameTime;
		double lastFPS;
	};

	enum
	{
		NO_OSD=0,
		CAMERA_OSD,
		NETWORK_OSD,
		GEOMETRY_OSD,
		TEXTURES_OSD,
		TAG_OSD,
		CONTROLLER_OSD,
		NUM_OSDS
	};
	//! Timestamp of when the system started.
	extern avs::Timestamp platformStartTimestamp;	
	//! Base class for a renderer that draws for a specific server.
	//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
	class Renderer:public SessionCommandInterface
	{
	public:
		Renderer(clientrender::NodeManager *localNodeManager,clientrender::NodeManager *remoteNodeManager,SessionClient *sessionClient,bool dev_mode);

		void SetMinimumPriority(int32_t p)
		{
			minimumPriority=p;
		}
		int32_t GetMinimumPriority() const
		{
			return minimumPriority;
		}
		virtual void ConfigureVideo(const avs::VideoConfig &vc)=0;
		virtual void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext)=0;
		avs::SetupCommand lastSetupCommand;
		avs::SetupLightingCommand lastSetupLightingCommand;

		float framerate = 0.0f;
		void Update(double timestamp_ms);
	protected:
		// determined by the stream setup command:
		vec4 colourOffsetScale;
		vec4 depthOffsetScale;

		bool keydown[256] = {};
		SessionClient *sessionClient=nullptr;
		teleport::core::Input inputs;
		/// A pointer to RenderPlatform, so that we can use the platform::crossplatform API.
		platform::crossplatform::RenderPlatform *renderPlatform	=nullptr;
		/// A framebuffer to store the colour and depth textures for the view.
		platform::crossplatform::BaseFramebuffer	*hdrFramebuffer	=nullptr;
		/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
		platform::crossplatform::HdrRenderer		*hDRRenderer	=nullptr;

		// A simple example mesh to draw as transparent
		platform::crossplatform::Effect *pbrEffect				= nullptr;
		platform::crossplatform::Effect *cubemapClearEffect	= nullptr;
		platform::crossplatform::ShaderResource _RWTagDataIDBuffer;
		platform::crossplatform::ShaderResource _lights;
		platform::crossplatform::ConstantBuffer<CameraConstants> cameraConstants;
		platform::crossplatform::StructuredBuffer<uint4> tagDataIDBuffer;
		platform::crossplatform::Texture* diffuseCubemapTexture	= nullptr;
		platform::crossplatform::Texture* specularCubemapTexture	= nullptr;
		platform::crossplatform::Texture* lightingCubemapTexture	= nullptr;
		platform::crossplatform::Texture* videoTexture				= nullptr;
		int show_osd = NO_OSD;
		double previousTimestamp=0.0;
		int32_t minimumPriority=0;
		bool using_vr = true;
		clientrender::GeometryCache localGeometryCache;
		clientrender::ResourceCreator localResourceCreator;

		int RenderMode=0;
		std::shared_ptr<clientrender::Material> mFlatColourMaterial;
		unsigned long long receivedInitialPos = 0;
		unsigned long long receivedRelativePos = 0;
		bool videoPosDecoded=false;
		bool canConnect=false;
		vec3 videoPos;

		avs::vec3 bodyOffsetFromHead; //Offset of player body from head pose.
		bool dev_mode = false;

		static constexpr float HFOV = 90;
		float gamma=0.44f;

		avs::uid node_select=0;
		bool have_vr_device = false;
		platform::crossplatform::Texture* externalTexture = nullptr;
		teleport::client::OpenXR *openXR=nullptr;
		avs::uid show_only=0;
		bool Match(const std::string& full_string, const std::string& substring);
	public:
		teleport::client::ClientPipeline clientPipeline;
		clientrender::GeometryCache geometryCache;
		clientrender::ResourceCreator resourceCreator;
		std::vector<avs::InputDefinition> inputDefinitions;
		static constexpr bool AudioStream	= true;
		static constexpr bool GeoStream		= true;
		static constexpr uint32_t NominalJitterBufferLength = 0;
		static constexpr uint32_t MaxJitterBufferLength = 50;

		//static constexpr avs::SurfaceFormat SurfaceFormat = avs::SurfaceFormat::ARGB;
		AVSTextureHandle avsTexture;

		GeometryDecoder geometryDecoder;
	
		bool render_local_offline = false;
	};
}