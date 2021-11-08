#pragma once

#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/camera_constants.sl"
#include "Shaders/cubemap_constants.sl"
#include "Shaders/pbr_constants.sl"
#include "Shaders/video_types.sl"

#include <libavstream/libavstream.hpp>
#include <libavstream/surfaces/surface_interface.hpp>
#include <libavstream/geometry/mesh_interface.hpp>
#include <libavstream/audio/audiotarget.h>

#include "SCR_Class_PC_Impl/PC_RenderPlatform.h"
#include "crossplatform/SessionClient.h"
#include "crossplatform/ResourceCreator.h"
#include "crossplatform/ResourceManager.h"
#include "crossplatform/GeometryDecoder.h"
#include "api/IndexBuffer.h"
#include "api/Shader.h"
#include "api/Texture.h"
#include "api/UniformBuffer.h"
#include "api/VertexBuffer.h"

#include "crossplatform/AudioStreamTarget.h"
#include "pc/PC_AudioPlayer.h"
#include "TeleportClient/ClientDeviceState.h"
#include "SCR_Class_PC_Impl/PC_MemoryUtil.h"
#include "Gui.h"

namespace avs
{
	typedef LARGE_INTEGER Timestamp;
}

namespace pc_client
{
	class IndexBuffer;
	class Shader;
	class Texture;
	class UniformBuffer;
	class VertexBuffer;
	class PC_RenderPlatform;
}

namespace scr
{
	class Material;
}
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

/// @brief The renderer for a client connection.
class ClientRenderer :public simul::crossplatform::PlatformRendererInterface, public SessionCommandInterface
{
	enum class ShaderMode
	{
		PBR, ALBEDO, NORMAL_UNSWIZZLED, DEBUG_ANIM, LIGHTMAPS, NORMAL_VERTEXNORMALS
	};

	void ChangePass(ShaderMode newShaderMode);

	/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
	/// distributes numerical precision to where it is better used.
	static const bool reverseDepth = true;
	/// A pointer to RenderPlatform, so that we can use the simul::crossplatform API.
	simul::crossplatform::RenderPlatform *renderPlatform	=nullptr;
	pc_client::PC_RenderPlatform PcClientRenderPlatform;
	/// A framebuffer to store the colour and depth textures for the view.
	simul::crossplatform::BaseFramebuffer	*hdrFramebuffer	=nullptr;
	/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
	simul::crossplatform::HdrRenderer		*hDRRenderer	=nullptr;

	// A simple example mesh to draw as transparent
	simul::crossplatform::Mesh *transparentMesh			= nullptr;
	simul::crossplatform::MeshRenderer *meshRenderer	= nullptr;
	simul::crossplatform::Effect *pbrEffect				= nullptr;
	simul::crossplatform::Effect *cubemapClearEffect	= nullptr;
	simul::crossplatform::ShaderResource _RWTagDataIDBuffer;
	simul::crossplatform::ShaderResource _lights;
	simul::crossplatform::ConstantBuffer<CubemapConstants> cubemapConstants;
	simul::crossplatform::ConstantBuffer<PbrConstants> pbrConstants;
	simul::crossplatform::ConstantBuffer<CameraConstants> cameraConstants;
	simul::crossplatform::ConstantBuffer<BoneMatrices> boneMatrices;
	simul::crossplatform::StructuredBuffer<uint4> tagDataIDBuffer;
	simul::crossplatform::StructuredBuffer<VideoTagDataCube> tagDataCubeBuffer;
	simul::crossplatform::StructuredBuffer<PbrLight> lightsBuffer;
	simul::crossplatform::Texture* diffuseCubemapTexture	= nullptr;
	simul::crossplatform::Texture* specularCubemapTexture	= nullptr;
	simul::crossplatform::Texture* lightingCubemapTexture	= nullptr;
	simul::crossplatform::Texture* videoTexture				= nullptr;

	static constexpr int maxTagDataSize = 32;
	VideoTagDataCube videoTagDataCube[maxTagDataSize];

	std::vector<scr::SceneCaptureCubeTagData> videoTagDataCubeArray;

	/// A camera instance to generate view and proj matrices and handle mouse control.
	/// In practice you will have your own solution for this.
	simul::crossplatform::Camera			camera;
	simul::crossplatform::MouseCameraState	mouseCameraState;
	simul::crossplatform::MouseCameraInput	mouseCameraInput;

	// determined by the stream setup command:
	vec4 colourOffsetScale;
	vec4 depthOffsetScale;

	bool keydown[256] = {};
	SessionClient sessionClient;
	ControllerState controllerStates[2];
	float framerate = 0.0f;

	avs::Timestamp platformStartTimestamp;	//Timestamp of when the system started.
	double previousTimestamp;				//Milliseconds since the state was last updated.

	scr::GeometryCache localGeometryCache;
	scr::GeometryCache geometryCache;
	
	bool show_video = false;
	bool renderPlayer = true; //Whether to render the player.

	enum
	{
		NO_OSD,
		CAMERA_OSD,
		NETWORK_OSD,
		GEOMETRY_OSD,
		TAG_OSD,
		CONTROLLER_OSD,
		NUM_OSDS
	};
	int show_osd = NETWORK_OSD;
	bool render_from_video_centre	= false;
	bool show_textures				= false;
	bool show_cubemaps				=false;

	std::string overridePassName = ""; //Pass used for rendering geometry.

	struct ControllerSim
	{
		vec3 controller_dir;
		vec3 view_dir;
		float angle = 0.0f;
		vec3 pos_offset[2];
		avs::vec3 position[2];
		avs::vec4 orientation[2];
	};
	ControllerSim controllerSim;
	uint32_t nextEventID = 0;

	std::string server_ip;
	int server_discovery_port=0;

	float roomRadius=1.5f;
	teleport::client::ClientDeviceState *clientDeviceState = nullptr;


	// handler for the UI to tell us to connect.
	void ConnectButtonHandler(const std::string& url);
public:
	ClientRenderer(teleport::client::ClientDeviceState *clientDeviceState, teleport::Gui &g);
	~ClientRenderer();
	// Implement SessionCommandInterface
	bool OnSetupCommandReceived(const char* server_ip, const avs::SetupCommand &setupCommand, avs::Handshake& handshake) override;
	void OnVideoStreamClosed() override;

	void OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand) override;

	bool OnNodeEnteredBounds(avs::uid nodeID) override;
	bool OnNodeLeftBounds(avs::uid nodeID) override;
	
	void OnLightingSetupChanged(const avs::SetupLightingCommand &l) override;

	std::vector<avs::uid> GetGeometryResources() override;
	void ClearGeometryResources() override;

	void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) override;
	void UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList) override;
	void UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList) override;
	void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
	void UpdateNodeAnimation(const avs::ApplyAnimation& animationUpdate) override;
	void UpdateNodeAnimationControl(const avs::NodeUpdateAnimationControl& animationControlUpdate) override;
	void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) override;

	// This allows live-recompile of shaders. 
	void RecompileShaders();
	void PrintHelpText(simul::crossplatform::GraphicsDeviceContext& deviceContext);
	
	void DrawOSD(simul::crossplatform::GraphicsDeviceContext& deviceContext);
	void WriteHierarchy(int tab,std::shared_ptr<scr::Node> node);
	void WriteHierarchies();
	void RenderLocalNodes(simul::crossplatform::GraphicsDeviceContext& deviceContext);
	void RenderNode(simul::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<scr::Node>& node);
	void RenderNodeOverlay(simul::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<scr::Node>& node);
	void RenderView(simul::crossplatform::GraphicsDeviceContext& deviceContext);

	int AddView();
	void ResizeView(int view_id, int W, int H);
	void Render(int view_id,void* context,void* renderTexture,int w,int h, long long frame, void* context_allocator = nullptr) override;
	void Init(simul::crossplatform::RenderPlatform *r);
	void SetServer(const char* ip_port, uint32_t clientID);
	void InvalidateDeviceObjects();
	void RemoveView(int);
	bool OnDeviceRemoved();
	void OnFrameMove(double fTime, float time_step, bool have_headset);
	void OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
	void OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta);
	void OnMouseMove(int xPos, int yPos,bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
	void OnKeyboard(unsigned wParam, bool bKeyDown, bool gui_shown);

	void CreateTexture(AVSTextureHandle &th,int width, int height, avs::SurfaceFormat format);
	void FillInControllerPose(int index, float offset);
	//Update the state of objects on the ClientRenderer.
	void Update();

	static constexpr bool AudioStream	= true;
	static constexpr bool GeoStream		= true;
	static constexpr uint32_t NominalJitterBufferLength = 0;
	static constexpr uint32_t MaxJitterBufferLength = 50;

	static constexpr avs::SurfaceFormat SurfaceFormat = avs::SurfaceFormat::ARGB;
	AVSTextureHandle avsTexture;
	avs::Context context;
	avs::VideoConfig videoConfig;

	avs::NetworkSource source;
	avs::Queue videoQueue;
	avs::Decoder decoder;
	avs::Surface surface;

	avs::Queue tagDataQueue;
	avs::TagDataDecoder tagDataDecoder;

	GeometryDecoder geometryDecoder;
	ResourceCreator resourceCreator;
	avs::Queue geometryQueue;
	avs::GeometryDecoder avsGeometryDecoder;
	avs::GeometryTarget avsGeometryTarget;
	
	avs::Queue audioQueue;
	avs::AudioDecoder avsAudioDecoder;
	avs::AudioTarget avsAudioTarget;
	std::unique_ptr<sca::AudioStreamTarget> audioStreamTarget;
	sca::PC_AudioPlayer audioPlayer;

	avs::DecoderParams decoderParams = {};
	avs::Pipeline pipeline;
	
	avs::SetupCommand lastSetupCommand;

	avs::SetupLightingCommand lastSetupLightingCommand;
	int RenderMode;
	std::shared_ptr<scr::Material> mFlatColourMaterial;
	unsigned long long receivedInitialPos = 0;
	unsigned long long receivedRelativePos = 0;
	bool videoPosDecoded=false;
	bool canConnect=false;
	vec3 videoPos;

	avs::vec3 bodyOffsetFromHead; //Offset of player body from head pose.
private:
	avs::uid show_only=0;
	void ListNode(simul::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<scr::Node>& node, int indent, int& linesRemaining);
	void OnReceiveVideoTagData(const uint8_t* data, size_t dataSize);
	void UpdateTagDataBuffers(simul::crossplatform::GraphicsDeviceContext& deviceContext);
	void RecomposeVideoTexture(simul::crossplatform::GraphicsDeviceContext& deviceContext, simul::crossplatform::Texture* srcTexture, simul::crossplatform::Texture* targetTexture, const char* technique);
	void RenderVideoTexture(simul::crossplatform::GraphicsDeviceContext& deviceContext, simul::crossplatform::Texture* srcTexture, simul::crossplatform::Texture* targetTexture, const char* technique, const char* shaderTexture, const simul::math::Matrix4x4& invCamMatrix);
	void RecomposeCubemap(simul::crossplatform::GraphicsDeviceContext& deviceContext, simul::crossplatform::Texture* srcTexture, simul::crossplatform::Texture* targetTexture, int mips, int2 sourceOffset);

	const PC_MemoryUtil memoryUtil;

	static constexpr float HFOV = 90;
	float gamma=0.44f;

	teleport::Gui &gui;
};
