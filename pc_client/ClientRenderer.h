#pragma once

#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/HdrRenderer.h"
#include "Simul/Platform/CrossPlatform/MeshRenderer.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/camera_constants.sl"
#include "SessionClient.h"

class ClientRenderer :public simul::crossplatform::PlatformRendererInterface, public SessionCommandInterface
{
	int frame_number;
	/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
	/// distributes numerical precision to where it is better used.
	static const bool reverseDepth = true;
	/// A pointer to RenderPlatform, so that we can use the simul::crossplatform API.
	simul::crossplatform::RenderPlatform *renderPlatform;
	/// A framebuffer to store the colour and depth textures for the view.
	simul::crossplatform::BaseFramebuffer	*hdrFramebuffer;
	/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
	simul::crossplatform::HdrRenderer		*hDRRenderer;

	// A simple example mesh to draw as transparent
	simul::crossplatform::Mesh *transparentMesh;
	simul::crossplatform::MeshRenderer *meshRenderer;
	simul::crossplatform::Effect *transparentEffect;
	simul::crossplatform::Effect *cubemapClearEffect;
	simul::crossplatform::ConstantBuffer<SolidConstants> solidConstants;
	simul::crossplatform::ConstantBuffer<CameraConstants> cameraConstants;
	simul::crossplatform::Texture *diffuseCubemapTexture;
	simul::crossplatform::Texture *specularTexture;
	/// A camera instance to generate view and proj matrices and handle mouse control.
	/// In practice you will have your own solution for this.
	simul::crossplatform::Camera			camera;
	simul::crossplatform::MouseCameraState	mouseCameraState;
	simul::crossplatform::MouseCameraInput	mouseCameraInput;
	bool keydown[256];
	int framenumber;
public:
	ClientRenderer();
	~ClientRenderer();
	// Implement SessionCommandInterface
	void OnVideoStreamChanged(uint port, uint width, uint height) override;
	void OnVideoStreamClosed() override;
	// This allows live-recompile of shaders. 
	void RecompileShaders();
	void GenerateCubemaps();
	int AddView();
	void ResizeView(int view_id, int W, int H);
	void RenderOpaqueTest(simul::crossplatform::DeviceContext &deviceContext);
	/// Render an example transparent object.
	void RenderTransparentTest(simul::crossplatform::DeviceContext &deviceContext);
	void Render(int view_id,void* context,void* renderTexture,int w,int h) override;
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
};
