#pragma once
#ifdef _MSC_VER

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

#include "TeleportClient/SessionClient.h"
#include "ClientRender/ResourceCreator.h"
#include "ClientRender/ResourceManager.h"
#include "ClientRender/IndexBuffer.h"
#include "ClientRender/Shader.h"
#include "ClientRender/Texture.h"
#include "ClientRender/UniformBuffer.h"
#include "ClientRender/VertexBuffer.h"
#include "ClientRender/Renderer.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportClient/ClientDeviceState.h"

#ifdef _MSC_VER
#include "crossplatform/AudioStreamTarget.h"
#include "SCR_Class_PC_Impl/PC_RenderPlatform.h"
#include "pc/PC_AudioPlayer.h"
#include "SCR_Class_PC_Impl/PC_MemoryUtil.h"
#endif

#include "Gui.h"
#include "TeleportClient/ClientPipeline.h"
#include "TeleportAudio/src/crossplatform/NetworkPipeline.h"

namespace avs
{
	typedef LARGE_INTEGER Timestamp;
}

namespace clientrender
{
	class Material;
}

namespace pc_client
{
	class IndexBuffer;
	class Shader;
	class Texture;
	class UniformBuffer;
	class VertexBuffer;
#ifdef _MSC_VER
	class PC_RenderPlatform;
#endif
}
namespace teleport
{
/// @brief The renderer for a client connection.
class ClientRenderer :public platform::crossplatform::PlatformRendererInterface, public SessionCommandInterface
		,public clientrender::Renderer
{
	enum class ShaderMode
	{
		PBR, ALBEDO, NORMAL_UNSWIZZLED, DEBUG_ANIM, LIGHTMAPS, NORMAL_VERTEXNORMALS
	};
	enum MouseOrKey :char
	{
		LEFT_BUTTON = 0x01
		, MIDDLE_BUTTON = 0x02
		, RIGHT_BUTTON = 0x04
	};

	void ChangePass(ShaderMode newShaderMode);

	/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
	/// distributes numerical precision to where it is better used.
	static const bool reverseDepth = true;
	pc_client::PC_RenderPlatform PcClientRenderPlatform;
	
	platform::crossplatform::ConstantBuffer<CubemapConstants> cubemapConstants;
	platform::crossplatform::ConstantBuffer<PbrConstants> pbrConstants;
	platform::crossplatform::ConstantBuffer<BoneMatrices> boneMatrices;
	platform::crossplatform::StructuredBuffer<VideoTagDataCube> tagDataCubeBuffer;
	platform::crossplatform::StructuredBuffer<PbrLight> lightsBuffer;
	static constexpr int maxTagDataSize = 32;
	VideoTagDataCube videoTagDataCube[maxTagDataSize];

	std::vector<clientrender::SceneCaptureCubeTagData> videoTagDataCubeArray;

	/// A camera instance to generate view and proj matrices and handle mouse control.
	/// In practice you will have your own solution for this.
	platform::crossplatform::Camera			camera;
	platform::crossplatform::MouseCameraState	mouseCameraState;
	platform::crossplatform::MouseCameraInput	mouseCameraInput;
	std::map<MouseOrKey, avs::InputId> inputIdMappings;

	// determined by the stream setup command:
	vec4 colourOffsetScale;
	vec4 depthOffsetScale;

	bool keydown[256] = {};
	SessionClient sessionClient;
	teleport::core::Input inputs;
	
	bool show_video = false;

	bool render_from_video_centre	= false;
	bool show_textures				= false;
	bool show_cubemaps				=false;
	bool show_node_overlays			=false;

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

	std::string server_ip;
	int server_discovery_port=0;

	teleport::client::ClientDeviceState *clientDeviceState = nullptr;

	// handler for the UI to tell us to connect.
	void ConnectButtonHandler(const std::string& url);

	// TODO: temporary.
	avs::uid server_uid=1;
public:
	ClientRenderer(teleport::client::ClientDeviceState *clientDeviceState, teleport::Gui &g,bool dev);
	~ClientRenderer();
	// Implement SessionCommandInterface
	bool OnSetupCommandReceived(const char* server_ip, const avs::SetupCommand &setupCommand, avs::Handshake& handshake) override;
	void ConfigureVideo(const avs::VideoConfig& vc) override;
	void OnVideoStreamClosed() override;

	void OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand) override;

	bool OnNodeEnteredBounds(avs::uid nodeID) override;
	bool OnNodeLeftBounds(avs::uid nodeID) override;
	
	void OnLightingSetupChanged(const avs::SetupLightingCommand &l) override;
	void OnInputsSetupChanged(const std::vector<avs::InputDefinition>& inputDefinitions) override;
	void UpdateNodeStructure(const avs::UpdateNodeStructureCommand& updateNodeStructureCommand) override;
	void UpdateNodeSubtype(const avs::UpdateNodeSubtypeCommand &updateNodeSubtypeCommand,const std::string &regexPath) override;

	std::vector<avs::uid> GetGeometryResources() override;
	void ClearGeometryResources() override;

	void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) override;
	void UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList) override;
	void UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList) override;
	void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
	void UpdateNodeAnimation(const avs::ApplyAnimation& animationUpdate) override;
	void UpdateNodeAnimationControl(const avs::NodeUpdateAnimationControl& animationControlUpdate) override;
	void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) override;
	
	// to render the vr view instead of re-rendering.
	void SetExternalTexture(platform::crossplatform::Texture* t);
	// This allows live-recompile of shaders. 
	void RecompileShaders();
	void PrintHelpText(platform::crossplatform::GraphicsDeviceContext& deviceContext);
	
	void DrawOSD(platform::crossplatform::GraphicsDeviceContext& deviceContext);
	void WriteHierarchy(int tab,std::shared_ptr<clientrender::Node> node);
	void WriteHierarchies();
	void RenderLocalNodes(platform::crossplatform::GraphicsDeviceContext& deviceContext,avs::uid server_uid,clientrender::GeometryCache &g);
	void RenderNode(platform::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<clientrender::Node>& node,clientrender::GeometryCache &g,bool force=false);
	void RenderNodeOverlay(platform::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<clientrender::Node>& node,clientrender::GeometryCache &g,bool force=false);
	void RenderView(platform::crossplatform::GraphicsDeviceContext& deviceContext);

	int AddView();
	void ResizeView(int view_id, int W, int H);
	void Render(int view_id,void* context,void* renderTexture,int w,int h, long long frame, void* context_allocator = nullptr) override;
	void Init(platform::crossplatform::RenderPlatform *r,teleport::client::OpenXR *u);
	void SetServer(const char* ip_port);
	void InvalidateDeviceObjects();
	void RemoveView(int);
	bool OnDeviceRemoved();
	void OnFrameMove(double fTime, float time_step, bool have_headset);
	void OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
	void OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta);
	void OnMouseMove(int xPos, int yPos,bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
	void OnKeyboard(unsigned wParam, bool bKeyDown, bool gui_shown);

	void CreateTexture(AVSTextureHandle &th,int width, int height);
	void FillInControllerPose(int index, float offset);
	std::unique_ptr<sca::AudioStreamTarget> audioStreamTarget;
	sca::PC_AudioPlayer audioPlayer;
	
	teleport::client::ClientPipeline clientPipeline;
	std::unique_ptr<sca::NetworkPipeline> inputNetworkPipeline;
	avs::Queue audioInputQueue;

	int RenderMode;
	std::shared_ptr<clientrender::Material> mFlatColourMaterial;
	unsigned long long receivedInitialPos = 0;
	unsigned long long receivedRelativePos = 0;
	bool videoPosDecoded=false;
	bool canConnect=false;
	vec3 videoPos;

	avs::vec3 bodyOffsetFromHead; //Offset of player body from head pose.
	bool dev_mode = false;
	bool render_local_offline = false;
private:
	avs::uid show_only=0;
	void OnReceiveVideoTagData(const uint8_t* data, size_t dataSize);
	void UpdateTagDataBuffers(platform::crossplatform::GraphicsDeviceContext& deviceContext);
	void RecomposeVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique);
	void RenderVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique, const char* shaderTexture, const platform::math::Matrix4x4& invCamMatrix);
	void RecomposeCubemap(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, int mips, int2 sourceOffset);

	const PC_MemoryUtil memoryUtil;

	static constexpr float HFOV = 90;
	float gamma=0.44f;

	teleport::Gui &gui;
	avs::uid node_select=0;
	bool have_vr_device = false;
	platform::crossplatform::Texture* externalTexture = nullptr;
	teleport::client::OpenXR *openXR=nullptr;
};

}
#endif