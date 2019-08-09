#define NOMINMAX				// Prevent Windows from defining min and max as macros
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Platform/CrossPlatform/Material.h"
#include "Simul/Platform/CrossPlatform/HDRRenderer.h"
#include "Simul/Platform/CrossPlatform/View.h"
#include "Simul/Platform/CrossPlatform/Mesh.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/CommandLineParams.h"
#include "Simul/Platform/CrossPlatform/SphericalHarmonics.h"

#include "SessionClient.h"
#include "Config.h"

#include <random>
#include <libavstream/surfaces/surface_dx11.hpp>

#include "libavstream/platforms/platform_windows.hpp"
#include "crossplatform/ResourceManager.h"
#include "api/IndexBuffer.h"
#include "api/Shader.h"
#include "api/Texture.h"
#include "api/UniformBuffer.h"
#include "api/VertexBuffer.h"

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


struct AVSTextureImpl :public AVSTexture
{
	AVSTextureImpl(simul::crossplatform::Texture *t)
		:texture(t)
	{
	}
	simul::crossplatform::Texture *texture = nullptr;
	avs::SurfaceBackendInterface* createSurface() const override
	{
		return new avs::SurfaceDX11(texture->AsD3D11Texture2D());
	}
};

void msgHandler(avs::LogSeverity severity, const char* msg, void* userData)
{
	if (severity > avs::LogSeverity::Warning)
		std::cerr << msg;
	else
		std::cout << msg ;
}

ClientRenderer::ClientRenderer():
	renderPlatform(nullptr),
	hdrFramebuffer(nullptr),
	hDRRenderer(nullptr),
	meshRenderer(nullptr),
	transparentMesh(nullptr),
	transparentEffect(nullptr),
	cubemapClearEffect(nullptr),
	specularTexture(nullptr),
	diffuseCubemapTexture(nullptr),
	framenumber(0),
	sessionClient(this),
	RenderMode(0),
	indexBufferManager(ResourceManager<scr::IndexBuffer*>(&scr::IndexBuffer::Destroy)),
	shaderManager(ResourceManager<scr::Shader*>(nullptr)),
	textureManager(ResourceManager<scr::Texture*>(&scr::Texture::Destroy)),
	uniformBufferManager(ResourceManager<scr::UniformBuffer*>(&scr::UniformBuffer::Destroy)),
	vertexBufferManager(ResourceManager<scr::VertexBuffer*>(&scr::VertexBuffer::Destroy))
{
	avsTextures.resize(NumStreams);

	//Initalise time stamping for state update.
	platformStartTimestamp = avs::PlatformWindows::getTimestamp();
	previousTimestamp = (uint32_t)avs::PlatformWindows::getTimeElapsed(platformStartTimestamp, avs::PlatformWindows::getTimestamp());
}

ClientRenderer::~ClientRenderer()
{	
	InvalidateDeviceObjects(); 
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
	//sessionClient.Connect(REMOTEPLAY_SERVER_IP,REMOTEPLAY_SERVER_PORT,REMOTEPLAY_TIMEOUT);

	avs::Context::instance()->setMessageHandler(msgHandler,nullptr);
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

void ClientRenderer::Render(int view_id,void* context,void* renderTexture,int w,int h, long long frame)
{
	simul::crossplatform::DeviceContext	deviceContext;
	deviceContext.setDefaultRenderTargets(renderTexture,
		nullptr,
		0,
		0,
		w,
		h
	);
	static simul::base::Timer timer;
	static float last_t = 0.0f;
	timer.UpdateTime();
	if (last_t != 0.0f&&timer.TimeSum!= last_t)
	{
		framerate=1000.0f/(timer.TimeSum -last_t);
	}
	last_t = timer.TimeSum;
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
			AVSTextureHandle th = avsTextures[0];
			AVSTexture &tx = *th;
			AVSTextureImpl *ti = static_cast<AVSTextureImpl*>(&tx);
			if (ti)
			{
				cubemapClearEffect->SetTexture(deviceContext, "plainTexture", ti->texture);
				cubemapClearEffect->Apply(deviceContext, RenderMode==1?"show_texture":"normal_view", 0);
				renderPlatform->DrawQuad(deviceContext);
				cubemapClearEffect->Unapply(deviceContext);
			}
		}

		//RenderOpaqueTest(deviceContext);

		// We must deactivate the depth buffer here, in order to use it as a texture:
		hdrFramebuffer->DeactivateDepth(deviceContext);

	}
	//renderPlatform->DrawTexture(deviceContext, 125, 125, 225, 225, diffuseCubemapTexture, vec4(0.5, 0.5, 0.0, 0.5));
	float s = 2.0f/ float(specularTexture->mips);
	for (int i = 0; i < specularTexture->mips; i++)
	{
		float lod = (float)i;
		float x = -1.0f + 0.5f*s+ i*s;
	//	renderPlatform->DrawCubemap(deviceContext, specularTexture, x, -.5, s, 1.0f, 1.0f, lod);
	}

	hdrFramebuffer->Deactivate(deviceContext);
	hDRRenderer->Render(deviceContext,hdrFramebuffer->GetTexture(),1.0f,0.44f);

	SIMUL_COMBINED_PROFILE_END(deviceContext)
	renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
	cpuProfiler.EndFrame();
/*	const char *txt=renderPlatform->GetGpuProfiler()->GetDebugText();
	renderPlatform->Print(deviceContext,0,0,txt);
	txt=cpuProfiler.GetDebugText();
	renderPlatform->Print(deviceContext,w/2,0,txt);*/
	{
		const avs::NetworkSourceCounters counters = source.getCounterValues();
		//ImGui::Text("Frame #: %d", renderStats.frameCounter);
		//ImGui::PlotLines("FPS", statFPS.data(), statFPS.count(), 0, nullptr, 0.0f, 60.0f);
		int y = 0;
		int dy = 18;
		renderPlatform->Print(deviceContext, w / 2, y += dy, sessionClient.IsConnected()? simul::base::QuickFormat("Connected to: %s"
			,sessionClient.GetServerIP().c_str()):"Not connected",vec4(1.f,1.f,1.f,1.f));
		renderPlatform->Print(deviceContext, w / 2, y += dy, simul::base::QuickFormat("Framerate: %4.4f", framerate));
		renderPlatform->Print(deviceContext, w / 2, y += dy, simul::base::QuickFormat("Start timestamp: %d", pipeline.GetStartTimestamp()));
		renderPlatform->Print(deviceContext, w / 2, y += dy, simul::base::QuickFormat("Current timestamp: %d",pipeline.GetTimestamp()));
		renderPlatform->Print(deviceContext, w / 2, y += dy, simul::base::QuickFormat("Bandwidth: %4.4f", counters.bandwidthKPS));
		renderPlatform->Print(deviceContext,w/2,y+=dy,simul::base::QuickFormat("Jitter Buffer Length: %d ", counters.jitterBufferLength ));
		renderPlatform->Print(deviceContext, w / 2, y += dy, simul::base::QuickFormat("Jitter Buffer Push: %d ", counters.jitterBufferPush));
		renderPlatform->Print(deviceContext,w/2,y+=dy,simul::base::QuickFormat("Jitter Buffer Pop: %d ", counters.jitterBufferPop )); 
		renderPlatform->Print(deviceContext,w/2,y+=dy, simul::base::QuickFormat("Network packets received: %d", counters.networkPacketsReceived));
		renderPlatform->Print(deviceContext,w/2,y+=dy,simul::base::QuickFormat("Network Packet orphans: %d", counters.m_packetMapOrphans));
		renderPlatform->Print(deviceContext,w/2,y+=dy,simul::base::QuickFormat("Max age: %d", counters.m_maxAge));
		renderPlatform->Print(deviceContext,w/2,y+=dy,simul::base::QuickFormat("Decoder packets received: %d", counters.decoderPacketsReceived));
		renderPlatform->Print(deviceContext,w/2,y+=dy,simul::base::QuickFormat("Network packets dropped: %d", counters.networkPacketsDropped));
		renderPlatform->Print(deviceContext,w/2,y+=dy,simul::base::QuickFormat("Decoder packets dropped: %d", counters.decoderPacketsDropped)); 
		//ImGui::PlotLines("Jitter buffer length", statJitterBuffer.data(), statJitterBuffer.count(), 0, nullptr, 0.0f, 100.0f);
		//ImGui::PlotLines("Jitter buffer push calls", statJitterPush.data(), statJitterPush.count(), 0, nullptr, 0.0f, 5.0f);
		//ImGui::PlotLines("Jitter buffer pop calls", statJitterPop.data(), statJitterPop.count(), 0, nullptr, 0.0f, 5.0f);
	}
	frame_number++;
}

void ClientRenderer::InvalidateDeviceObjects()
{
	for (auto i : avsTextures)
	{
		AVSTextureImpl *ti = (AVSTextureImpl*)i.get();
		if (ti)
		{
			SAFE_DELETE(ti->texture);
		}
	}
	avsTextures.clear();
	if(transparentEffect)
	{
		transparentEffect->InvalidateDeviceObjects();
		delete transparentEffect;
		transparentEffect=nullptr;
	}
	if(hDRRenderer)
		hDRRenderer->InvalidateDeviceObjects();
	if(renderPlatform)
		renderPlatform->InvalidateDeviceObjects();
	if(hdrFramebuffer)
		hdrFramebuffer->InvalidateDeviceObjects();
	if(meshRenderer)
		meshRenderer->InvalidateDeviceObjects();
	SAFE_DELETE(transparentMesh);
	SAFE_DELETE(diffuseCubemapTexture);
	SAFE_DELETE(specularTexture);
	SAFE_DELETE(meshRenderer);
	SAFE_DELETE(hDRRenderer);
	SAFE_DELETE(hdrFramebuffer);
	SAFE_DELETE(transparentEffect);
	SAFE_DELETE(cubemapClearEffect);
	SAFE_DELETE(diffuseCubemapTexture);
	SAFE_DELETE(specularTexture);
}

void ClientRenderer::RemoveView(int)
{
}

bool ClientRenderer::OnDeviceRemoved()
{
	InvalidateDeviceObjects();
	return true;
}

void ClientRenderer::CreateTexture(AVSTextureHandle &th,int width, int height, avs::SurfaceFormat format)
{
	if (!(th))
		th.reset(new AVSTextureImpl(nullptr));
	AVSTexture *t = th.get();
	AVSTextureImpl *ti=(AVSTextureImpl*)t;
	if(!ti->texture)
		ti->texture = renderPlatform->CreateTexture();
	ti->texture->ensureTexture2DSizeAndFormat(renderPlatform, width, height, simul::crossplatform::RGBA_8_UNORM, true, true, false);
}

void ClientRenderer::Update()
{
	uint32_t timestamp = (uint32_t)avs::PlatformWindows::getTimeElapsed(platformStartTimestamp, avs::PlatformWindows::getTimestamp());
	uint32_t timeElapsed = (timestamp - previousTimestamp);

	indexBufferManager.Update(timeElapsed);
	shaderManager.Update(timeElapsed);
	textureManager.Update(timeElapsed);
	uniformBufferManager.Update(timeElapsed);
	vertexBufferManager.Update(timeElapsed);

	previousTimestamp = timestamp;
}

void ClientRenderer::OnVideoStreamChanged(uint remotePort, uint width, uint height)
{
    WARN("VIDEO STREAM CHANGED: %d %d %d", remotePort, width, height);

	sourceParams.nominalJitterBufferLength = NominalJitterBufferLength;
	sourceParams.maxJitterBufferLength = MaxJitterBufferLength;
	// Configure for num video streams + 1 geometry stream
	if (!source.configure(NumStreams+(GeoStream?1:0), remotePort+1, "127.0.0.1", remotePort, sourceParams))
	{
		LOG("Failed to configure network source node");
		return;
	}

	decoderParams.codec = avs::VideoCodec::HEVC;
	decoderParams.deferDisplay = true;
	avs::DeviceHandle dev;
	dev.handle = renderPlatform->AsD3D11Device();
	dev.type = avs::DeviceType::Direct3D11;

	pipeline.reset();
	// Top of the pipeline, we have the network source.
	pipeline.add(&source);

	for (auto t : avsTextures)
	{
		AVSTextureImpl* ti= (AVSTextureImpl*)(t.get());
		if (ti)
		{
			SAFE_DELETE(ti->texture);
		}
	}
	/* Now for each stream, we add both a DECODER and a SURFACE node. e.g. for two streams:
					 /->decoder -> surface
			source -<
					 \->decoder	-> surface
	*/
	for (size_t i = 0; i < NumStreams; ++i)
	{
		CreateTexture(avsTextures[i],width, height, SurfaceFormats[i]);
		// Video streams are 50+...
		if (!decoder[i].configure(dev, width, height, decoderParams, (int)(50+i)))
		{
			throw std::runtime_error("Failed to configure decoder node");
		}
		if (!surface[i].configure(avsTextures[i]->createSurface()))
		{
			throw std::runtime_error("Failed to configure output surface node");
		}

		pipeline.link({ &decoder[i], &surface[i] });
		avs::Node::link(source, decoder[i]);
	}
	// We will add a GEOMETRY PIPE:
	if(GeoStream)
	{
		avsGeometryDecoder.configure(100,&geometryDecoder);
		avsGeometryTarget.configure(&resourceCreator);
		pipeline.link({ &source, &avsGeometryDecoder, &avsGeometryTarget });
	}
	//java->Env->CallVoidMethod(java->ActivityObject, jni.initializeVideoStreamMethod, port, width, height, mVideoSurfaceTexture->GetJavaObject());
}

void ClientRenderer::OnVideoStreamClosed()
{
    WARN("VIDEO STREAM CLOSED");
	pipeline.deconfigure();
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
	controllerState.mTrackpadX=0.5f;
	controllerState.mTrackpadY=0.5f;
	controllerState.mTrackpadStatus=true;
	// Handle networked session.
	if (sessionClient.IsConnected())
	{
		vec3 forward=-camera.GetOrientation().Tz();
		//std::cout << forward.x << " " << forward.y << " " << forward.z << "\n";
		// The camera has Z backward, X right, Y up.
		// But we want orientation relative to X right, Y forward, Z up.
		simul::math::Quaternion q0(3.1415926536f / 2.f, simul::math::Vector3(1.f,0.0f, 0.0f));
		auto q = camera.GetOrientation().GetQuaternion();
		auto q_rel=q / q0;
		sessionClient.Frame(q_rel,controllerState);
		pipeline.process();

		static short c = 0;
		if (!c--)
		{
			const avs::NetworkSourceCounters Counters = source.getCounterValues();
			std::cout << "Network packets dropped: " << 100.0f*Counters.networkDropped << "%"
				<< "\nDecoder packets dropped: " << 100.0f*Counters.decoderDropped << "%"
				<< std::endl;
		}
	}
	else
	{
		ENetAddress remoteEndpoint;
		if (sessionClient.Discover(REMOTEPLAY_DISCOVERY_PORT, remoteEndpoint))
		{
			sessionClient.Connect(remoteEndpoint, REMOTEPLAY_TIMEOUT);
		}
	}

	sessionClient.Frame(camera.GetOrientation().GetQuaternion(), controllerState);
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
		case 'K':
			sessionClient.Disconnect(0);
			break;
		case VK_LEFT: 
		case VK_RIGHT: 
		case VK_UP: 
		case VK_DOWN:
			break;
		case 'M':
			RenderMode++;
			RenderMode= RenderMode%2;
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
