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
#include "Log.h"

#include <algorithm>
#include <random>
#include <libavstream/surfaces/surface_dx11.hpp>

#include "libavstream/platforms/platform_windows.hpp"

#include "crossplatform/Material.h"

#include "SCR_Class_PC_Impl/PC_Texture.h"

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
	pbrEffect(nullptr),
	cubemapClearEffect(nullptr),
	specularTexture(nullptr),
	videoAsCubemapTexture(nullptr),
	dummyDiffuse(nullptr),
	dummyNormal(nullptr),
	dummyCombined(nullptr),
	diffuseCubemapTexture(nullptr),
	framenumber(0),
	sessionClient(this),
	resourceCreator(basist::transcoder_texture_format::cTFBC1),
	RenderMode(0)
{
	avsTextures.resize(NumStreams);
	resourceCreator.SetRenderPlatform(&PcClientRenderPlatform);
	resourceCreator.AssociateResourceManagers(&resourceManagers.mIndexBufferManager, &resourceManagers.mShaderManager, &resourceManagers.mMaterialManager, &resourceManagers.mTextureManager, &resourceManagers.mUniformBufferManager, &resourceManagers.mVertexBufferManager);
	resourceCreator.AssociateActorManager(&resourceManagers.mActorManager);

	//Initalise time stamping for state update.
	platformStartTimestamp = avs::PlatformWindows::getTimestamp();
	previousTimestamp = (uint32_t)avs::PlatformWindows::getTimeElapsed(platformStartTimestamp, avs::PlatformWindows::getTimestamp());
}

ClientRenderer::~ClientRenderer()
{
	pipeline.deconfigure();
	InvalidateDeviceObjects(); 
}

void ClientRenderer::Init(simul::crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	PcClientRenderPlatform.SetSimulRenderPlatform(r);
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

	// These are for example:
	hDRRenderer->RestoreDeviceObjects(renderPlatform);
	meshRenderer->RestoreDeviceObjects(renderPlatform);
	hdrFramebuffer->RestoreDeviceObjects(renderPlatform);

	videoAsCubemapTexture = renderPlatform->CreateTexture();
	// dummy textures for materials:
	dummyDiffuse = renderPlatform->CreateTexture();
	dummyNormal = renderPlatform->CreateTexture();
	dummyCombined = renderPlatform->CreateTexture();
	dummyDiffuse->ensureTexture2DSizeAndFormat(renderPlatform, 1, 1, crossplatform::PixelFormat::BGRA_8_UNORM);
	uint32_t white = 0xFFFFFFFF;
	dummyDiffuse->setTexels(renderPlatform->GetImmediateContext(), &white, 0, 1);

	dummyNormal->ensureTexture2DSizeAndFormat(renderPlatform, 1, 1, crossplatform::PixelFormat::BGRA_8_UNORM);
	uint32_t blue = 0x00FF0000;
	dummyNormal->setTexels(renderPlatform->GetImmediateContext(), &blue, 0, 1);

	dummyCombined->ensureTexture2DSizeAndFormat(renderPlatform, 1, 1, crossplatform::PixelFormat::BGRA_8_UNORM);
	dummyCombined->setTexels(renderPlatform->GetImmediateContext(), &white, 0, 1);

	errno=0;
	RecompileShaders();

	pbrConstants.RestoreDeviceObjects(renderPlatform);
	pbrConstants.LinkToEffect(pbrEffect,"pbrConstants");
	cubemapConstants.RestoreDeviceObjects(renderPlatform);
	cubemapConstants.LinkToEffect(cubemapClearEffect, "CubemapConstants");
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
	delete pbrEffect;
	delete cubemapClearEffect;
	pbrEffect = renderPlatform->CreateEffect("pbr");
	cubemapClearEffect = renderPlatform->CreateEffect("cubemap_clear");
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
	if(!pbrEffect)
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
		pbrConstants.reverseDepth		=deviceContext.viewStruct.frustum.reverseDepth;
		mat4 m = mat4::identity();
		meshRenderer->Render(deviceContext, transparentMesh,*(mat4*)&model,diffuseCubemapTexture,specularTexture);
	}
}

base::DefaultProfiler cpuProfiler;
/// Render an example transparent object.
void ClientRenderer::RenderTransparentTest(crossplatform::DeviceContext &deviceContext)
{
	if (!pbrEffect)
		return;
	// Vertical position
	static float z = 500.0f;
	simul::math::Matrix4x4 model = math::Matrix4x4::Translation(0, 0, z);
	// scale.
	static float sc[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	model.ScaleRows(sc);
	pbrConstants.reverseDepth = deviceContext.viewStruct.frustum.reverseDepth;
	// Some arbitrary light values 
	pbrConstants.lightIrradiance = vec3(12, 12, 12);
	pbrConstants.lightDir = vec3(0, 0, 1);
	mat4 m = mat4::identity();
	meshRenderer->Render(deviceContext, transparentMesh, m, diffuseCubemapTexture, specularTexture);
}

void ClientRenderer::Render(int view_id, void* context, void* renderTexture, int w, int h, long long frame)
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
	if (last_t != 0.0f && timer.TimeSum != last_t)
	{
		framerate = 1000.0f / (timer.TimeSum - last_t);
	}
	last_t = timer.TimeSum;
	deviceContext.platform_context = context;
	deviceContext.renderPlatform = renderPlatform;
	deviceContext.viewStruct.view_id = view_id;
	deviceContext.viewStruct.depthTextureStyle = crossplatform::PROJECTION;
	simul::crossplatform::SetGpuProfilingInterface(deviceContext, renderPlatform->GetGpuProfiler());
	simul::base::SetProfilingInterface(GET_THREAD_ID(), &cpuProfiler);
	renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
	cpuProfiler.SetMaxLevel(5);
	cpuProfiler.StartFrame();
	renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
	SIMUL_COMBINED_PROFILE_START(deviceContext, "all");

	crossplatform::Viewport viewport = renderPlatform->GetViewport(deviceContext, 0);

	hdrFramebuffer->Activate(deviceContext);
	hdrFramebuffer->Clear(deviceContext, 0.0f, 0.0f, 1.0f, 0.f, reverseDepth ? 0.f : 1.f);

	pbrEffect->UnbindTextures(deviceContext);
	// The following block renders to the hdrFramebuffer's rendertarget:
	{
		deviceContext.viewStruct.view = camera.MakeViewMatrix();
		float aspect = (float)viewport.w / (float)viewport.h;
		if (reverseDepth)
			deviceContext.viewStruct.proj = camera.MakeDepthReversedProjectionMatrix(aspect);
		else
			deviceContext.viewStruct.proj = camera.MakeProjectionMatrix(aspect);
		// MUST call init each frame.
		deviceContext.viewStruct.Init();
		AVSTextureHandle th = avsTextures[0];
		AVSTexture& tx = *th;
		AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
		if (ti)
		{
			{
				cubemapConstants.sourceOffset = int2(0, 0);
				cubemapClearEffect->SetTexture(deviceContext, "plainTexture", ti->texture);
				cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
				cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
				cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", videoAsCubemapTexture);

				cubemapClearEffect->Apply(deviceContext, "recompose_with_depth_alpha", 0);
				renderPlatform->DispatchCompute(deviceContext, videoAsCubemapTexture->width / 16, videoAsCubemapTexture->width / 16, 6);
				cubemapClearEffect->Unapply(deviceContext);
				cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", nullptr);
				cubemapClearEffect->UnbindTextures(deviceContext);

			}
		}
		{
			cubemapConstants.depthOffsetScale = depthOffsetScale;
			cubemapConstants.colourOffsetScale = colourOffsetScale;
			cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
			cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
			cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
			cubemapClearEffect->SetTexture(deviceContext, "cubemapTexture", videoAsCubemapTexture);
			if (ti)
			{
				cubemapClearEffect->SetTexture(deviceContext, "plainTexture", ti->texture);
				cubemapClearEffect->Apply(deviceContext, "use_cubemap", 0);
				renderPlatform->DrawQuad(deviceContext);
				cubemapClearEffect->Unapply(deviceContext);


				renderPlatform->DrawTexture(deviceContext, 0, 0, hdrFramebuffer->GetWidth() / 2, hdrFramebuffer->GetHeight() / 2, ti->texture);
			}
		}
//		RenderLocalActors(deviceContext);

		// We must deactivate the depth buffer here, in order to use it as a texture:
		hdrFramebuffer->DeactivateDepth(deviceContext);

	}
	{
		auto& textures = resourceCreator.GetTextureManager()->GetCache();
		static int tw = 32;
		int x = 0, y = 0;
		if(sessionClient.IsConnected())
		for (auto t : textures)
		{
			pc_client::PC_Texture* pct = static_cast<pc_client::PC_Texture*>(&(*t.second.resource));
			renderPlatform->DrawTexture(deviceContext, x, y, tw, tw, pct->GetSimulTexture());
			x += tw;
			if (x > hdrFramebuffer->GetWidth() - tw)
			{
				x = 0;
				y += tw;
			}
		}
		y += tw;
		renderPlatform->DrawTexture(deviceContext, x+=tw, y, tw, tw, dummyDiffuse);
		renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, dummyNormal);
		renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, dummyCombined);
	}
	hdrFramebuffer->Deactivate(deviceContext);
	hDRRenderer->Render(deviceContext,hdrFramebuffer->GetTexture(),1.0f,0.44f);

	SIMUL_COMBINED_PROFILE_END(deviceContext);
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
		vec4 white(1.f, 1.f, 1.f, 1.f);
		renderPlatform->Print(deviceContext, w / 2, y += dy, sessionClient.IsConnected()? simul::base::QuickFormat("Connected to: %s"
			,sessionClient.GetServerIP().c_str()):"Not connected",white);
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
		avs::Transform transform = decoder[0].getCameraTransform();
		renderPlatform->Print(deviceContext, w / 2, y += dy, simul::base::QuickFormat("Camera: %4.4f %4.4f %4.4f", transform.position.x, transform.position.y, transform.position.z),white);

		//ImGui::PlotLines("Jitter buffer length", statJitterBuffer.data(), statJitterBuffer.count(), 0, nullptr, 0.0f, 100.0f);
		//ImGui::PlotLines("Jitter buffer push calls", statJitterPush.data(), statJitterPush.count(), 0, nullptr, 0.0f, 5.0f);
		//ImGui::PlotLines("Jitter buffer pop calls", statJitterPop.data(), statJitterPop.count(), 0, nullptr, 0.0f, 5.0f);
	}
	frame_number++;
}


void ClientRenderer::RenderLocalActors(simul::crossplatform::DeviceContext& deviceContext)
{
	avs::Transform transform=decoder[0].getCameraTransform();
	vec3 pos = (const float*)& transform.position;
	camera.SetPosition(pos);
	//camera.SetOrientationAsQuaternion((const float*)&transform.rotation);
	deviceContext.viewStruct.view = camera.MakeViewMatrix();
	deviceContext.viewStruct.Init();
	for (auto& actor : resourceManagers.mActorManager.m_Actors)
	{
		const std::shared_ptr<scr::Transform> tr = actor.second->GetTransform();
		if (!tr)
			continue;
		const std::shared_ptr<scr::Mesh> mesh = actor.second->GetMesh();
		if (!mesh)
			continue;
		const std::vector<std::shared_ptr<scr::Material>> materials = actor.second->GetMaterials();
		if (!materials.size())
			continue;
		std::shared_ptr<scr::Transform> transform = actor.second->GetTransform();
		if (!transform)
			continue;
		const auto* vb = dynamic_cast<pc_client::PC_VertexBuffer*>(mesh->GetMeshCreateInfo().vb.get());
		const auto* ib = dynamic_cast<pc_client::PC_IndexBuffer*>(mesh->GetMeshCreateInfo().ib.get());

		const simul::crossplatform::Buffer* const v[] = { vb->GetSimulVertexBuffer() };

		const scr::Material::MaterialCreateInfo& m = materials[0]->GetMaterialCreateInfoConst();

		if (!layout)
		{
			simul::crossplatform::LayoutDesc desc[] =
			{
				{ "POSITION", 0, crossplatform::RGB_32_FLOAT, 0, 0, false, 0 },
				{ "NORMAL", 0, crossplatform::RGB_32_FLOAT, 0, 12, false, 0 },
				{ "TANGENT", 0, crossplatform::RGBA_32_FLOAT, 0, 24, false, 0 },
				{ "TEXCOORD", 0, crossplatform::RG_32_FLOAT, 0, 40, false, 0 },
				{ "TEXCOORD", 1, crossplatform::RG_32_FLOAT, 0, 48, false, 0 },
			};
			layout = renderPlatform->CreateLayout(
				sizeof(desc) / sizeof(simul::crossplatform::LayoutDesc)
				, desc);
		}
		cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
		mat4 model;
		model=((const float*)& (transform->GetTransformMatrix()));
		mat4::mul(cameraConstants.worldViewProj ,*((mat4*)&deviceContext.viewStruct.viewProj), model);
		cameraConstants.world = model;
		cameraConstants.viewPosition = vec3(0,0,0);

		scr::Material &mat = *materials[0];
		{
			auto& mcr = mat.GetMaterialCreateInfo();
			const scr::Material::MaterialData& md = mat.GetMaterialData();
			memcpy(&pbrConstants.diffuseOutputScalar, &md, sizeof(md));
			auto* d = ((pc_client::PC_Texture*) & (*mcr.diffuse.texture));
			auto* n = ((pc_client::PC_Texture*) & (*mcr.normal.texture));
			auto* c = ((pc_client::PC_Texture*) & (*mcr.combined.texture));


			pbrEffect->SetTexture(deviceContext,pbrEffect->GetShaderResource("diffuseTexture"),	d?d->GetSimulTexture():dummyDiffuse);
			pbrEffect->SetTexture(deviceContext,pbrEffect->GetShaderResource("normalTexture"),	n?n->GetSimulTexture():dummyNormal);
			pbrEffect->SetTexture(deviceContext,pbrEffect->GetShaderResource("combinedTexture"),c?c->GetSimulTexture():dummyCombined);
		}

		pbrEffect->SetConstantBuffer(deviceContext, &pbrConstants);
		pbrEffect->SetConstantBuffer(deviceContext, &cameraConstants);
		pbrEffect->Apply(deviceContext, pbrEffect->GetTechniqueByIndex(0), 0);
 		renderPlatform->SetLayout(deviceContext, layout);
		renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLELIST);
		renderPlatform->SetVertexBuffers(deviceContext, 0, 1, v, layout);
		renderPlatform->SetIndexBuffer(deviceContext, ib->GetSimulIndexBuffer());
		renderPlatform->DrawIndexed(deviceContext, (int)ib->GetIndexBufferCreateInfo().indexCount, 0, 0);
		layout->Unapply(deviceContext);
		pbrEffect->Unapply(deviceContext);

		float heightOffset = -80.0F;
		scr::mat4 inv_ue4ViewMatrix = scr::mat4::Translation(scr::vec3(-480.0F, -80.0F, -142.0F + heightOffset));
		scr::mat4 changeOfBasis = scr::mat4(scr::vec4(0.0F, 1.0F, 0.0F, 0.0F), scr::vec4(0.0F, 0.0F, 1.0F, 0.0F), scr::vec4(-1.0F, 0.0F, 0.0F, 0.0F), scr::vec4(0.0F, 0.0F, 0.0F, 1.0F));
		scr::mat4 scr_Transform = changeOfBasis * inv_ue4ViewMatrix;// *ic_mmm.pTransform->GetTransformMatrix();
	}
}
void ClientRenderer::InvalidateDeviceObjects()
{
	delete layout;
	layout = nullptr;
	for (auto i : avsTextures)
	{
		AVSTextureImpl *ti = (AVSTextureImpl*)i.get();
		if (ti)
		{
			SAFE_DELETE(ti->texture);
		}
	}
	avsTextures.clear();
	if(pbrEffect)
	{
		pbrEffect->InvalidateDeviceObjects();
		delete pbrEffect;
		pbrEffect=nullptr;
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
	SAFE_DELETE(videoAsCubemapTexture);
	SAFE_DELETE(dummyDiffuse);
	SAFE_DELETE(dummyNormal);
	SAFE_DELETE(dummyCombined);
	SAFE_DELETE(meshRenderer);
	SAFE_DELETE(hDRRenderer);
	SAFE_DELETE(hdrFramebuffer);
	SAFE_DELETE(pbrEffect);
	SAFE_DELETE(cubemapClearEffect);
	SAFE_DELETE(diffuseCubemapTexture);
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

	// disabled because it deletes objects that are in use!
	resourceManagers.Update(timeElapsed);

	previousTimestamp = timestamp;
}

void ClientRenderer::OnVideoStreamChanged(const avs::SetupCommand &setupCommand)
{
	WARN("VIDEO STREAM CHANGED: port %d clr %d x %d dpth %d x %d", setupCommand.port, setupCommand.video_width, setupCommand.video_height
																	,setupCommand.depth_width,setupCommand.depth_height	);

	sourceParams.nominalJitterBufferLength = NominalJitterBufferLength;
	sourceParams.maxJitterBufferLength = MaxJitterBufferLength;
	// Configure for num video streams + 1 geometry stream
	if (!source.configure(NumStreams+(GeoStream?1:0), setupCommand.port +1, "127.0.0.1", setupCommand.port, sourceParams))
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
	size_t stream_width =std::max(setupCommand.video_width,setupCommand.depth_width);
	size_t stream_height = setupCommand.video_height;

	videoAsCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, setupCommand.colour_cubemap_size, setupCommand.colour_cubemap_size, 1, 1,
		crossplatform::PixelFormat::RGBA_8_UNORM, true, false, true);


	colourOffsetScale.x=0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(setupCommand.video_height) / float(stream_height);
	
	depthOffsetScale.x = 0;
	depthOffsetScale.y = float(setupCommand.video_height) / float(stream_height);
	depthOffsetScale.z = float(setupCommand.depth_width) / float(stream_width);
	depthOffsetScale.w = float(setupCommand.depth_height) / float(stream_height);

	for (size_t i = 0; i < NumStreams; ++i)
	{
		CreateTexture(avsTextures[i], int(stream_width),int(stream_height), SurfaceFormats[i]);
		// Video streams are 50+...
		if (!decoder[i].configure(dev, (int)stream_width, (int)stream_height, decoderParams, (int)(50+i)))
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
	controllerState.mJoystickX =(mouseCameraInput.right_left_input);
	controllerState.mJoystickY =(mouseCameraInput.forward_back_input);
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
		auto q_rel=q/q0;
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

	//sessionClient.Frame(camera.GetOrientation().GetQuaternion(), controllerState);
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
			break;
		default: 
			int  k=tolower(wParam);
			if(k>255)
				return;
			keydown[k]=bKeyDown?1:0;
		break; 
	}
}
