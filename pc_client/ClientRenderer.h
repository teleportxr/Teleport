#pragma once

#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/HdrRenderer.h"
#include "Simul/Platform/CrossPlatform/MeshRenderer.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/camera_constants.sl"
#include "Shaders/cubemap_constants.sl"
#include "Shaders/pbr_constants.sl"

#include <libavstream/libavstream.hpp>
#include <libavstream/surfaces/surface_interface.hpp>
#include <libavstream/geometrydecoder.hpp>

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

class ClientRenderer :public simul::crossplatform::PlatformRendererInterface, public SessionCommandInterface
{
	enum class ShaderMode
	{
		PBR, ALBEDO, NORMAL_UNSWIZZLED, NORMAL_UNREAL, NORMAL_UNITY
	};

	void ChangePass(ShaderMode newShaderMode);

	int frame_number;
	/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
	/// distributes numerical precision to where it is better used.
	static const bool reverseDepth = true;
	/// A pointer to RenderPlatform, so that we can use the simul::crossplatform API.
	simul::crossplatform::RenderPlatform *renderPlatform;
	pc_client::PC_RenderPlatform PcClientRenderPlatform;
	/// A framebuffer to store the colour and depth textures for the view.
	simul::crossplatform::BaseFramebuffer	*hdrFramebuffer;
	/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
	simul::crossplatform::HdrRenderer		*hDRRenderer;

	// A simple example mesh to draw as transparent
	simul::crossplatform::Mesh *transparentMesh;
	simul::crossplatform::MeshRenderer *meshRenderer;
	simul::crossplatform::Effect *pbrEffect;
	simul::crossplatform::Effect *cubemapClearEffect;
	simul::crossplatform::ConstantBuffer<CubemapConstants> cubemapConstants;
	simul::crossplatform::ConstantBuffer<PbrConstants> pbrConstants;
	simul::crossplatform::ConstantBuffer<CameraConstants> cameraConstants;
	simul::crossplatform::StructuredBuffer<vec4>		cameraPositionBuffer;
	simul::crossplatform::Texture *diffuseCubemapTexture;
	simul::crossplatform::Texture *specularCubemapTexture;
	simul::crossplatform::Texture* roughSpecularCubemapTexture;
	simul::crossplatform::Texture* lightingCubemapTexture;
	simul::crossplatform::Texture* videoAsCubemapTexture;
	/// A camera instance to generate view and proj matrices and handle mouse control.
	/// In practice you will have your own solution for this.
	simul::crossplatform::Camera			camera;
	simul::crossplatform::MouseCameraState	mouseCameraState;
	simul::crossplatform::MouseCameraInput	mouseCameraInput;
	// determined by the stream setup command:
	vec4 colourOffsetScale;
	vec4 depthOffsetScale;

	bool keydown[256];
	int framenumber;
	SessionClient sessionClient;
	ControllerState controllerState;
	float framerate = 0.0f;

	avs::Timestamp platformStartTimestamp; //Timestamp of when the system started.
	uint32_t previousTimestamp; //Milliseconds since system started from when the state was last updated.
	
	scr::ResourceManagers resourceManagers;
	void Recompose(simul::crossplatform::DeviceContext& deviceContext, simul::crossplatform::Texture* srcTexture, simul::crossplatform::Texture* targetTexture, int mips, int2 sourceOffset);
	bool show_video = false;
	bool show_osd = true;
	bool render_from_video_centre = false;
	bool show_textures = false;

	std::string passName = "pbr"; //Pass used for rendering geometry.
public:
	ClientRenderer();
	~ClientRenderer();
	// Implement SessionCommandInterface
	void OnVideoStreamChanged(const avs::SetupCommand &setupCommand, avs::Handshake& handshake, bool shouldClearEverything, std::vector<avs::uid>& resourcesClientNeeds, std::vector<avs::uid>& outExistingActors) override;
	void OnVideoStreamClosed() override;

	virtual bool OnActorEnteredBounds(avs::uid actor_uid) override;
	virtual bool OnActorLeftBounds(avs::uid actor_uid) override;
	// This allows live-recompile of shaders. 
	void RecompileShaders();
	void RenderLocalActors(simul::crossplatform::DeviceContext &);
	int AddView();
	void ResizeView(int view_id, int W, int H);
	void RenderOpaqueTest(simul::crossplatform::DeviceContext &deviceContext);
	/// Render an example transparent object.
	void RenderTransparentTest(simul::crossplatform::DeviceContext &deviceContext);
	void Render(int view_id,void* context,void* renderTexture,int w,int h, long long frame) override;
	void Init(simul::crossplatform::RenderPlatform *r);
	void InvalidateDeviceObjects();
	void RemoveView(int);
	bool OnDeviceRemoved();
	void OnFrameMove(double fTime, float time_step);
	void OnMouse(bool bLeftButtonDown
		, bool bRightButtonDown
		, bool bMiddleButtonDown
		, int nMouseWheelDelta
		, int xPos
		, int yPos);
	void OnKeyboard(unsigned wParam, bool bKeyDown);

	void CreateTexture(AVSTextureHandle &th,int width, int height, avs::SurfaceFormat format);

	//Update the state of objects on the ClientRenderer.
	void Update();

	static constexpr size_t NumStreams = 1;
	static constexpr bool GeoStream  = true;
	static constexpr uint32_t NominalJitterBufferLength = 0;
	static constexpr uint32_t MaxJitterBufferLength = 50;

	static constexpr avs::SurfaceFormat SurfaceFormats[2] = {
		avs::SurfaceFormat::ARGB10,
		avs::SurfaceFormat::ARGB,
	};

	std::vector<AVSTextureHandle> avsTextures;
	avs::Context context;

	avs::NetworkSource source;
	avs::Decoder decoder[NumStreams];
	avs::Surface surface[NumStreams];

	GeometryDecoder geometryDecoder;
	ResourceCreator resourceCreator;
	avs::GeometryDecoder avsGeometryDecoder;
	avs::GeometryTarget avsGeometryTarget;

	avs::NetworkSourceParams sourceParams = {};
	avs::DecoderParams decoderParams = {};
	avs::Pipeline pipeline;
	int RenderMode;
	std::shared_ptr<scr::Material> mFlatColourMaterial;
	bool receivedInitialPos = false;
	avs::vec3 oculusOrigin;
	vec3 videoPos;
};
