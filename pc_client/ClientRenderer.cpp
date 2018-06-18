#define NOMINMAX				// Prevent Windows from defining min and max as macros
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/Material.h"
#include "Simul/Platform/CrossPlatform/HDRRenderer.h"
#include "Simul/Platform/CrossPlatform/View.h"
#include "Simul/Platform/CrossPlatform/Mesh.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/CommandLineParams.h"
#include "Simul/Platform/CrossPlatform/SphericalHarmonics.h"

#include "SessionClient.h"

#include <random>

std::default_random_engine generator;
std::uniform_real_distribution<float> rando(-1.0f,1.f);

using namespace simul;



void set_float4(float f[4], float a, float b, float c, float d)
{
	f[0] = a;
	f[1] = b;
	f[2] = c;
	f[3] = d;
}


void apply_material()
{
}

// Windows Header Files:
#include <windows.h>
#include <SDKDDKVer.h>
#include <shellapi.h>
#include "ClientRenderer.h"

ClientRenderer::ClientRenderer():
	renderPlatform(nullptr)
	,hdrFramebuffer(nullptr)
	,hDRRenderer(nullptr)
	,meshRenderer(nullptr)
	,transparentMesh(nullptr)
	,transparentEffect(nullptr)
	,cubemapClearEffect(nullptr)
	,specularTexture(nullptr)
	,diffuseCubemapTexture(nullptr)
	,framenumber(0)
{
}

ClientRenderer::~ClientRenderer()
{	
	InvalidateDeviceObjects(); 
	delete meshRenderer;
	del(hDRRenderer,nullptr);
	del(hdrFramebuffer,nullptr);
}

void ClientRenderer::Init(simul::crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	hDRRenderer		=new crossplatform::HdrRenderer();

	hdrFramebuffer=renderPlatform->CreateFramebuffer();
	hdrFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
	hdrFramebuffer->SetDepthFormat(crossplatform::D_32_FLOAT);
	hdrFramebuffer->SetAntialiasing(1);
	meshRenderer = new crossplatform::MeshRenderer();
	camera.SetPositionAsXYZ(0.f,0.f,5.f);
	vec3 look(0.f,1.f,0.f),up(0.f,0.f,1.f);
	camera.LookInDirection(look,up);
	camera.SetHorizontalFieldOfViewDegrees(90.f);
	// Automatic vertical fov - depends on window shape:
	camera.SetVerticalFieldOfViewDegrees(0.f);

	crossplatform::CameraViewStruct vs;
	vs.exposure=1.f;
	vs.farZ=300000.f;
	vs.nearZ=1.f;
	vs.gamma=0.44f;
	vs.InfiniteFarPlane=false;
	vs.projection=crossplatform::DEPTH_REVERSE;

	memset(keydown,0,sizeof(keydown));
	// Use these in practice:
	//
	GenerateCubemaps();

	// These are for example:
	hDRRenderer->RestoreDeviceObjects(renderPlatform);
	meshRenderer->RestoreDeviceObjects(renderPlatform);
	hdrFramebuffer->RestoreDeviceObjects(renderPlatform);
	errno=0;
	RecompileShaders();
	solidConstants.RestoreDeviceObjects(renderPlatform);
	solidConstants.LinkToEffect(transparentEffect,"SolidConstants");
	cameraConstants.RestoreDeviceObjects(renderPlatform);
	// Create a basic cube.
	transparentMesh=renderPlatform->CreateMesh();
}

// This allows live-recompile of shaders. 
void ClientRenderer::RecompileShaders()
{
	renderPlatform->RecompileShaders();
	hDRRenderer->RecompileShaders();
	meshRenderer->RecompileShaders();
	delete transparentEffect;
	delete cubemapClearEffect;
	transparentEffect = renderPlatform->CreateEffect("solid");
	cubemapClearEffect = renderPlatform->CreateEffect("cubemap_clear");
}

void ClientRenderer::GenerateCubemaps()
{
	crossplatform::Texture *hdrTexture = renderPlatform->CreateTexture("textures/glacier.hdr");
	delete specularTexture;
	specularTexture = renderPlatform->CreateTexture("specularTexture");
	specularTexture->ensureTextureArraySizeAndFormat(renderPlatform, 1024, 1024, 1, 8, crossplatform::PixelFormat::RGBA_16_FLOAT, true, true, true);
	// plonk the hdr into the cubemap.
	auto &deviceContext = renderPlatform->GetImmediateContext();
	renderPlatform->LatLongTextureToCubemap(deviceContext, specularTexture, hdrTexture);
	delete hdrTexture;
	delete diffuseCubemapTexture;
	diffuseCubemapTexture = renderPlatform->CreateTexture("diffuseCubemapTexture");
	diffuseCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, 32, 32, 1, 1, crossplatform::PixelFormat::RGBA_16_FLOAT, true, true, true);

	crossplatform::SphericalHarmonics  sphericalHarmonics;

	// Now we will calculate spherical harmonics.
	sphericalHarmonics.RestoreDeviceObjects(renderPlatform);
	sphericalHarmonics.RenderMipsByRoughness(deviceContext, specularTexture);
	sphericalHarmonics.CalcSphericalHarmonics(deviceContext, specularTexture);
	// And using the harmonics, render a diffuse map:
	sphericalHarmonics.RenderEnvmap(deviceContext, diffuseCubemapTexture, -1, 0.0f);
}

// We only ever create one view in this example, but in general, this should return a new value each time it's called.
int ClientRenderer::AddView(/*external_framebuffer*/)
{
	static int last_view_id=0;
	// We override external_framebuffer here and pass "true" to demonstrate how external depth buffers are used.
	// In this case, we use hdrFramebuffer's depth buffer.
	return last_view_id++;
}
void ClientRenderer::ResizeView(int view_id,int W,int H)
{
	hDRRenderer->SetBufferSize(W,H);
	hdrFramebuffer->SetWidthAndHeight(W,H);
	hdrFramebuffer->SetAntialiasing(1);
}

/// Render an example transparent object.
void ClientRenderer::RenderOpaqueTest(crossplatform::DeviceContext &deviceContext)
{
	if(!transparentEffect)
		return;
	generator.seed(12);
	static float spd=20.0f;
	static float angle=0.0f;
	angle+=0.01f;
	// Vertical position
	simul::math::Matrix4x4 model;
	for(int i=0;i<1000;i++)
	{
		float x=100.0f*(rando(generator));
		float y=100.0f*(rando(generator));
		float z=10.0f*rando(generator);
		float vx=rando(generator)*spd;
		float vy=rando(generator)*spd;
		float vz=rando(generator)*spd*.1f;
		float sn=sin(angle+rando(generator)*3.1415926536f);
		x+=vx*sn;
		y+=vy*sn;
		z+=vz*sn;
		model=math::Matrix4x4::Translation(x,y,z);
		// scale.
		static float sc[]={1.0f,1.0f,1.0f,1.0f};
		model.ScaleRows(sc);
		solidConstants.reverseDepth		=deviceContext.viewStruct.frustum.reverseDepth;
		mat4 m = mat4::identity();
		meshRenderer->Render(deviceContext, transparentMesh,*(mat4*)&model,diffuseCubemapTexture,specularTexture);
	}
}

base::DefaultProfiler cpuProfiler;
/// Render an example transparent object.
void ClientRenderer::RenderTransparentTest(crossplatform::DeviceContext &deviceContext)
{
	if (!transparentEffect)
		return;
	// Vertical position
	static float z = 500.0f;
	simul::math::Matrix4x4 model = math::Matrix4x4::Translation(0, 0, z);
	// scale.
	static float sc[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	model.ScaleRows(sc);
	solidConstants.reverseDepth = deviceContext.viewStruct.frustum.reverseDepth;
	// Some arbitrary light values 
	solidConstants.lightIrradiance = vec3(12, 12, 12);
	solidConstants.lightDir = vec3(0, 0, 1);
	mat4 m = mat4::identity();
	meshRenderer->Render(deviceContext, transparentMesh, m, diffuseCubemapTexture, specularTexture);
}

void ClientRenderer::Render(int view_id,void* context,void* renderTexture,int w,int h)
{
	simul::crossplatform::DeviceContext	deviceContext;
	deviceContext.setDefaultRenderTargets(renderTexture,
		nullptr,
		0,
		0,
		w,
		h
	);
	deviceContext.platform_context	= context;
	deviceContext.renderPlatform	=renderPlatform;
	deviceContext.viewStruct.view_id=view_id;
	deviceContext.viewStruct.depthTextureStyle	=crossplatform::PROJECTION;
	simul::crossplatform::SetGpuProfilingInterface(deviceContext,renderPlatform->GetGpuProfiler());
	simul::base::SetProfilingInterface(GET_THREAD_ID(),&cpuProfiler);
	renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
	cpuProfiler.SetMaxLevel(5);
	cpuProfiler.StartFrame();
	renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
	SIMUL_COMBINED_PROFILE_START(deviceContext, "all")

	crossplatform::Viewport viewport = renderPlatform->GetViewport(deviceContext, 0);
	
	hdrFramebuffer->Activate(deviceContext);
	hdrFramebuffer->Clear(deviceContext,0.1f,0.1f,0.12f,0.f,reverseDepth?0.f:1.f);

	transparentEffect->UnbindTextures(deviceContext);
	// The following block renders to the hdrFramebuffer's rendertarget:
	{
		deviceContext.viewStruct.view=camera.MakeViewMatrix();
		float aspect=(float)viewport.w/(float)viewport.h;
		if(reverseDepth)
			deviceContext.viewStruct.proj=camera.MakeDepthReversedProjectionMatrix(aspect);
		else
			deviceContext.viewStruct.proj=camera.MakeProjectionMatrix(aspect);
		// MUST call init each frame.
		deviceContext.viewStruct.Init();

		{
			cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
			cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
			cubemapClearEffect->SetTexture(deviceContext, "cubemapTexture", specularTexture);
			cubemapClearEffect->Apply(deviceContext, "cubemap_clear", 0);
			renderPlatform->DrawQuad(deviceContext);
			cubemapClearEffect->Unapply(deviceContext);
		}

		RenderOpaqueTest(deviceContext);

		// We must deactivate the depth buffer here, in order to use it as a texture:
		hdrFramebuffer->DeactivateDepth(deviceContext);

	}
	//renderPlatform->DrawTexture(deviceContext, 125, 125, 225, 225, diffuseCubemapTexture, vec4(0.5, 0.5, 0.0, 0.5));
	float s = 2.0f/ float(specularTexture->mips);
	for (int i = 0; i < specularTexture->mips; i++)
	{
		float lod = i;
		float x = -1.0f + 0.5f*s+ i*s;
	//	renderPlatform->DrawCubemap(deviceContext, specularTexture, x, -.5, s, 1.0f, 1.0f, lod);
	}

	hdrFramebuffer->Deactivate(deviceContext);
	hDRRenderer->Render(deviceContext,hdrFramebuffer->GetTexture(),1.0f,0.44f);

	SIMUL_COMBINED_PROFILE_END(deviceContext)
	renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
	cpuProfiler.EndFrame();
	const char *txt=renderPlatform->GetGpuProfiler()->GetDebugText();
	renderPlatform->Print(deviceContext,0,0,txt);
	txt=cpuProfiler.GetDebugText();
	renderPlatform->Print(deviceContext,w/2,0,txt);
	frame_number++;
}

void ClientRenderer::InvalidateDeviceObjects()
{
	if(transparentEffect)
	{
		transparentEffect->InvalidateDeviceObjects();
		delete transparentEffect;
		transparentEffect=nullptr;
	}
	delete cubemapClearEffect;
	cubemapClearEffect = nullptr;
	if(hDRRenderer)
		hDRRenderer->InvalidateDeviceObjects();
	if(renderPlatform)
		renderPlatform->InvalidateDeviceObjects();
	if(hdrFramebuffer)
		hdrFramebuffer->InvalidateDeviceObjects();
	if(meshRenderer)
		meshRenderer->InvalidateDeviceObjects();
	delete transparentMesh;
	transparentMesh=nullptr;
	delete diffuseCubemapTexture;
	diffuseCubemapTexture=nullptr;
	delete specularTexture;
	specularTexture = nullptr;
}

void ClientRenderer::RemoveView(int)
{
}

bool ClientRenderer::OnDeviceRemoved()
{
	InvalidateDeviceObjects();
	return true;
}

void ClientRenderer::OnVideoStreamChanged(uint port, uint width, uint height)
{
    WARN("VIDEO STREAM CHANGED: %d %d %d", port, width, height);

	//const ovrJava* java = app->GetJava();
	//java->Env->CallVoidMethod(java->ActivityObject, jni.initializeVideoStreamMethod, port, width, height, mVideoSurfaceTexture->GetJavaObject());
}

void ClientRenderer::OnVideoStreamClosed()
{
    WARN("VIDEO STREAM CLOSED");

	//const ovrJava* java = app->GetJava();
	//java->Env->CallVoidMethod(java->ActivityObject, jni.closeVideoStreamMethod);
}


void ClientRenderer::OnFrameMove(double fTime,float time_step)
{
	mouseCameraInput.forward_back_input	=(float)keydown['w']-(float)keydown['s'];
	mouseCameraInput.right_left_input	=(float)keydown['d']-(float)keydown['a'];
	mouseCameraInput.up_down_input		=(float)keydown['t']-(float)keydown['g'];
	crossplatform::UpdateMouseCamera(&camera
							,time_step
							,20.f
							,mouseCameraState
							,mouseCameraInput
							,14000.f);
}

void ClientRenderer::OnMouse(bool bLeftButtonDown
			,bool bRightButtonDown
			,bool bMiddleButtonDown
            ,int nMouseWheelDelta
            ,int xPos
			,int yPos )
{
	mouseCameraInput.MouseButtons
		=(bLeftButtonDown?crossplatform::MouseCameraInput::LEFT_BUTTON:0)
		|(bRightButtonDown?crossplatform::MouseCameraInput::RIGHT_BUTTON:0)
		|(bMiddleButtonDown?crossplatform::MouseCameraInput::MIDDLE_BUTTON:0);
	mouseCameraInput.MouseX=xPos;
	mouseCameraInput.MouseY=yPos;
}

void ClientRenderer::OnKeyboard(unsigned wParam,bool bKeyDown)
{
	switch (wParam) 
	{ 
		case VK_LEFT: 
		case VK_RIGHT: 
		case VK_UP: 
		case VK_DOWN:
			break;
		case 'R':
			RecompileShaders();
			GenerateCubemaps();
			break;
		default: 
			int  k=tolower(wParam);
			if(k>255)
				return;
			keydown[k]=bKeyDown?1:0;
		break; 
	}
}
