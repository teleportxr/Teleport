#define NOMINMAX				// Prevent Windows from defining min and max as macros
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/Timer.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"
#include "Platform/CrossPlatform/Material.h"
#include "Platform/CrossPlatform/HDRRenderer.h"
#include "Platform/CrossPlatform/View.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/CommandLineParams.h"
#include "Platform/CrossPlatform/SphericalHarmonics.h"

#include "Config.h"
#include "PCDiscoveryService.h"

#include <algorithm>
#include <random>
#include <functional>

#include <libavstream/surfaces/surface_dx11.hpp>

#include "libavstream/platforms/platform_windows.hpp"

#include "crossplatform/Material.h"
#include "crossplatform/Log.h"
#include "crossplatform/SessionClient.h"

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
	specularCubemapTexture(nullptr),
	roughSpecularCubemapTexture(nullptr),
	lightingCubemapTexture(nullptr),
	videoTexture(nullptr),
	diffuseCubemapTexture(nullptr),
	framenumber(0),
	resourceManagers(new scr::ActorManager),
	resourceCreator(basist::transcoder_texture_format::cTFBC3),
	sessionClient(this, std::make_unique<PCDiscoveryService>()),
	RenderMode(0)
{
	sessionClient.SetResourceCreator(&resourceCreator);
	avsTextures.resize(NumVidStreams);
	resourceCreator.AssociateResourceManagers(resourceManagers);

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
	// Initialize the audio (asynchronously)
	audioPlayer.initializeAudioDevice();

	renderPlatform=r;
	PcClientRenderPlatform.SetSimulRenderPlatform(r);
	r->SetShaderBuildMode(crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
	resourceCreator.Initialise(&PcClientRenderPlatform, scr::VertexBufferLayout::PackingStyle::INTERLEAVED);

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
	vs.farZ=3000.f;
	vs.nearZ=0.01f;
	vs.gamma=1.0f;
	vs.InfiniteFarPlane=true;
	vs.projection=crossplatform::DEPTH_REVERSE;
	
	camera.SetCameraViewStruct(vs);

	memset(keydown,0,sizeof(keydown));
	// These are for example:
	hDRRenderer->RestoreDeviceObjects(renderPlatform);
	meshRenderer->RestoreDeviceObjects(renderPlatform);
	hdrFramebuffer->RestoreDeviceObjects(renderPlatform);

	videoTexture = renderPlatform->CreateTexture();
	specularCubemapTexture = renderPlatform->CreateTexture();
	roughSpecularCubemapTexture = renderPlatform->CreateTexture();
	diffuseCubemapTexture = renderPlatform->CreateTexture();
	lightingCubemapTexture = renderPlatform->CreateTexture();
	errno=0;
	RecompileShaders();

	pbrConstants.RestoreDeviceObjects(renderPlatform);
	pbrConstants.LinkToEffect(pbrEffect,"pbrConstants");
	cubemapConstants.RestoreDeviceObjects(renderPlatform);
	cubemapConstants.LinkToEffect(cubemapClearEffect, "CubemapConstants");
	cameraConstants.RestoreDeviceObjects(renderPlatform); 
	tagDataIDBuffer.RestoreDeviceObjects(renderPlatform, 1, true);
	tagData2DBuffer.RestoreDeviceObjects(renderPlatform, maxTagDataSize, false, true);
	tagDataCubeBuffer.RestoreDeviceObjects(renderPlatform, maxTagDataSize, false, true);
	lightsBuffer.RestoreDeviceObjects(renderPlatform,10,false,true);
	boneMatrices.RestoreDeviceObjects(renderPlatform);
	boneMatrices.LinkToEffect(pbrEffect, "boneMatrices");

	// Create a basic cube.
	transparentMesh=renderPlatform->CreateMesh();
	//sessionClient.Connect(REMOTEPLAY_SERVER_IP,REMOTEPLAY_SERVER_PORT,REMOTEPLAY_TIMEOUT);

	avs::Context::instance()->setMessageHandler(msgHandler,nullptr);
}

void ClientRenderer::SetServer(const char *ip,int port)
{
	server_ip=ip;
	server_discovery_port=port;
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
	_RWTagDataIDBuffer = cubemapClearEffect->GetShaderResource("RWTagDataIDBuffer");
	_lights = pbrEffect->GetShaderResource("lights");
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
	if(hDRRenderer)
		hDRRenderer->SetBufferSize(W,H);
	if(hdrFramebuffer)
	{
		hdrFramebuffer->SetWidthAndHeight(W,H);
		hdrFramebuffer->SetAntialiasing(1);
	}
}

/// Render an example transparent object.
void ClientRenderer::RenderOpaqueTest(crossplatform::GraphicsDeviceContext &deviceContext)
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
		meshRenderer->Render(deviceContext, transparentMesh,*(mat4*)&model,diffuseCubemapTexture, specularCubemapTexture);
	}
}

base::DefaultProfiler cpuProfiler;
/// Render an example transparent object.
void ClientRenderer::RenderTransparentTest(crossplatform::GraphicsDeviceContext &deviceContext)
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
	mat4 m = mat4::identity();
	meshRenderer->Render(deviceContext, transparentMesh, m, diffuseCubemapTexture, specularCubemapTexture);
}


void ClientRenderer::ChangePass(ShaderMode newShaderMode)
{
	switch(newShaderMode)
	{
		case ShaderMode::PBR:
			passName = "pbr";
			break;
		case ShaderMode::ALBEDO:
			passName = "albedo_only";
			break;
		case ShaderMode::NORMAL_UNSWIZZLED:
			passName = "normal_unswizzled";
			break;
		case ShaderMode::NORMAL_UNREAL:
			passName = "normal_unreal";
			break;
		case ShaderMode::NORMAL_UNITY:
			passName = "normal_unity";
			break;
		case ShaderMode::NORMAL_VERTEXNORMALS:
			passName = "normal_vertexnormals";
			break;
	}
}

void ClientRenderer::Recompose(simul::crossplatform::GraphicsDeviceContext &deviceContext, simul::crossplatform::Texture *srcTexture, simul::crossplatform::Texture* targetTexture, int mips,int2 sourceOffset)
{
	cubemapConstants.sourceOffset = sourceOffset ;
	cubemapClearEffect->SetTexture(deviceContext, "plainTexture", srcTexture);
	cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
	cubemapConstants.targetSize = targetTexture->width;
	for (int m = 0; m < mips; m++)
	{
		cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", targetTexture, -1, m);
		cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
		cubemapClearEffect->Apply(deviceContext, "recompose", 0);
		renderPlatform->DispatchCompute(deviceContext, targetTexture->width / 16, targetTexture->width / 16, 6);
		cubemapClearEffect->Unapply(deviceContext);
		cubemapConstants.sourceOffset.y += 2 * cubemapConstants.targetSize;
		cubemapConstants.targetSize /= 2;
	}
	cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", nullptr);
	cubemapClearEffect->UnbindTextures(deviceContext);
}

void ClientRenderer::Render(int view_id, void* context, void* renderTexture, int w, int h, long long frame)
{
	simul::crossplatform::GraphicsDeviceContext	deviceContext;
	deviceContext.setDefaultRenderTargets(renderTexture,
		nullptr,
		0,
		0,
		w,
		h
	);
	static simul::core::Timer timer;
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

	// 
	vec3 true_pos = camera.GetPosition();
	if (render_from_video_centre)
	{
		Sleep(200);
		vec3 pos = videoPosDecoded ? videoPos : vec3(0, 0, 0);
		camera.SetPosition(pos);
	}

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
			// This will apply to both rendering methods
			{
				cubemapClearEffect->SetTexture(deviceContext, "plainTexture", ti->texture); 
				tagDataIDBuffer.ApplyAsUnorderedAccessView(deviceContext, cubemapClearEffect, _RWTagDataIDBuffer );
				cubemapConstants.sourceOffset = int2(ti->texture->width - (32 * 4), ti->texture->length - 4);
				cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
				cubemapClearEffect->Apply(deviceContext, "extract_tag_data_id", 0);
				renderPlatform->DispatchCompute(deviceContext, 1, 1, 1);
				cubemapClearEffect->Unapply(deviceContext);
				cubemapClearEffect->UnbindTextures(deviceContext);

				tagDataIDBuffer.CopyToReadBuffer(deviceContext);
				const uint4* videoIDBuffer = tagDataIDBuffer.OpenReadBuffer(deviceContext);
				if (videoIDBuffer && videoIDBuffer[0].x < 32 && videoIDBuffer[0].w == 110) // sanity check
				{	
					int tagDataID = videoIDBuffer[0].x;	

					if (videoTexture->IsCubemap())
					{
						const auto& ct = videoTagDataCubeArray[tagDataID].coreData.cameraTransform;
						videoPos = vec3(ct.position.x, ct.position.y, ct.position.z);
					}
					else
					{
						const auto& ct = videoTagData2DArray[tagDataID].cameraTransform;
						videoPos = vec3(ct.position.x, ct.position.y, ct.position.z);
					}

					videoPosDecoded = true;
				}
				tagDataIDBuffer.CloseReadBuffer(deviceContext);
			}

			UpdateTagDataBuffers(deviceContext);

			if (videoTexture->IsCubemap())
			{
				int W = videoTexture->width;
				{
					cubemapConstants.sourceOffset = int2(0, 2 * W);
					cubemapClearEffect->SetTexture(deviceContext, "plainTexture", ti->texture);
					cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
					cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
					cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", videoTexture);

					cubemapClearEffect->Apply(deviceContext, "recompose_with_depth_alpha", 0);
					renderPlatform->DispatchCompute(deviceContext, W / 16, W / 16, 6);
					cubemapClearEffect->Unapply(deviceContext);
					cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", nullptr);
					cubemapClearEffect->UnbindTextures(deviceContext);
				}
				int2 sourceOffset(3 * W / 2, 2 * W);
				Recompose(deviceContext, ti->texture, diffuseCubemapTexture			, diffuseCubemapTexture->mips, int2(videoConfig.diffuse_x,videoConfig.diffuse_y));
				Recompose(deviceContext, ti->texture, specularCubemapTexture		, specularCubemapTexture->mips, int2(videoConfig.specular_x,videoConfig.specular_y));
				Recompose(deviceContext, ti->texture, roughSpecularCubemapTexture	, specularCubemapTexture->mips, int2(videoConfig.rough_x,videoConfig.rough_y));
				Recompose(deviceContext, ti->texture, lightingCubemapTexture		, lightingCubemapTexture->mips, int2(videoConfig.light_x,videoConfig.light_y));
				{
					tagDataCubeBuffer.Apply(deviceContext, cubemapClearEffect, cubemapClearEffect->GetShaderResource("TagDataCubeBuffer"));
					cubemapConstants.depthOffsetScale = vec4(0, 0, 0, 0);
					cubemapConstants.offsetFromVideo = vec3(camera.GetPosition()) - videoPos;
					cubemapConstants.cameraPosition = vec3(camera.GetPosition());
					cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
					cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
					cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
					cubemapClearEffect->SetTexture(deviceContext, "cubemapTexture", videoTexture);
					
					cubemapClearEffect->SetTexture(deviceContext, "plainTexture", ti->texture);
					cubemapClearEffect->Apply(deviceContext, "use_cubemap", 0);
					renderPlatform->DrawQuad(deviceContext);
					cubemapClearEffect->Unapply(deviceContext);
					cubemapClearEffect->UnbindTextures(deviceContext);
				}
				RenderLocalActors(deviceContext);
			}
			else
			{
				{
					int W = videoTexture->width;
					int H = videoTexture->length;
					tagData2DBuffer.Apply(deviceContext, cubemapClearEffect, cubemapClearEffect->GetShaderResource("TagData2DBuffer"));
					cubemapConstants.sourceOffset = int2(0, 0);
					cubemapClearEffect->SetTexture(deviceContext, "plainTexture", ti->texture);
					cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
					cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
					cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", videoTexture);

					cubemapClearEffect->Apply(deviceContext, "recompose_perspective", 0);
					renderPlatform->DispatchCompute(deviceContext, W / 16, H / 16, 1);
					cubemapClearEffect->Unapply(deviceContext);
					cubemapClearEffect->SetUnorderedAccessView(deviceContext, "RWTextureTargetArray", nullptr);
					cubemapClearEffect->UnbindTextures(deviceContext);
				}
				if (!show_video)
				{
					int W = hdrFramebuffer->GetWidth();
					int H = hdrFramebuffer->GetHeight();
					renderPlatform->DrawTexture(deviceContext, 0, 0, W, H, videoTexture);
				}
			}
			// We must deactivate the depth buffer here, in order to use it as a texture:
			hdrFramebuffer->DeactivateDepth(deviceContext);
			if (show_video)
			{
				int W = hdrFramebuffer->GetWidth();
				int H = hdrFramebuffer->GetHeight();
				renderPlatform->DrawTexture(deviceContext, 0, 0, W, H, ti->texture);
			}
			renderPlatform->DrawCubemap(deviceContext,diffuseCubemapTexture,-0.3f,0.5f,0.2f,1.f,1.f);
			renderPlatform->DrawCubemap(deviceContext,specularCubemapTexture,0.0f,0.5f,0.2f,1.f,1.f);
			renderPlatform->DrawCubemap(deviceContext,roughSpecularCubemapTexture,0.3f,0.5f,0.2f,1.f,1.f);
		}
	}
	vec4 white(1.f, 1.f, 1.f, 1.f);
	if (render_from_video_centre)
	{
		camera.SetPosition(true_pos);
		renderPlatform->Print(deviceContext, w-16, h-16, "C", white);
	}
	if(show_textures)
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto& textures = resourceManagers.mTextureManager.GetCache(cacheLock);
		static int tw = 64;
		int x = 0, y = hdrFramebuffer->GetHeight()-tw*2;
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
		renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, ((pc_client::PC_Texture*)((resourceCreator.m_DummyDiffuse.get())))->GetSimulTexture());
		renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, ((pc_client::PC_Texture*)((resourceCreator.m_DummyNormal.get())))->GetSimulTexture());
		renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, ((pc_client::PC_Texture*)((resourceCreator.m_DummyCombined.get())))->GetSimulTexture());
	}
	hdrFramebuffer->Deactivate(deviceContext);
	hDRRenderer->Render(deviceContext,hdrFramebuffer->GetTexture(),1.0f,0.44f);

	SIMUL_COMBINED_PROFILE_END(deviceContext);
	renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
	cpuProfiler.EndFrame();
	if(show_osd)
	{
		DrawOSD(deviceContext);
	}
	frame_number++;
}

void ClientRenderer::UpdateTagDataBuffers(simul::crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (lastSetupCommand.video_config.use_cubemap)
	{
		VideoTagDataCube data[maxTagDataSize];
		for (int i = 0; i < videoTagDataCubeArray.size(); ++i)
		{
			const auto& td = videoTagDataCubeArray[i];
			const auto& pos = td.coreData.cameraTransform.position;
			const auto& rot = td.coreData.cameraTransform.rotation;

			data[i].cameraPosition = { pos.x, pos.y, pos.z };
			data[i].cameraRotation = { rot.x, rot.y, rot.z, rot.w };
			for(int j=0;j<td.lights.size();j++)
			{
				LightTag &t=data[i].lightTags[j];
				const scr::LightData &l=td.lights[j];
				// Convert from +-1 to [0,1]
				t.shadowTexCoordOffset.x=float(l.texturePosition[0])/float(lastSetupCommand.video_config.video_width);
				t.shadowTexCoordOffset.y=float(l.texturePosition[1])/float(lastSetupCommand.video_config.video_height);
				t.shadowTexCoordScale.x=float(l.textureSize)/float(lastSetupCommand.video_config.video_width);
				t.shadowTexCoordScale.y=float(l.textureSize)/float(lastSetupCommand.video_config.video_height);
				// Because tag data is NOT properly transformed in advance yet:
				avs::vec3 position		=l.position;
				avs::vec4 orientation	=l.orientation;
				avs::ConvertPosition(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, position);
				avs::ConvertRotation(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, orientation);
				scr::mat4 worldToShadowMatrix=scr::mat4::Translation(position)*scr::mat4::Rotation(orientation) ;
			//	worldToShadowMatrix		=scr::mat4((const float*)&l.shadowProjectionMatrix)*worldToShadowMatrix;
				t.worldToShadowMatrix	=*((mat4*)&worldToShadowMatrix);
			}
		}	
		tagDataCubeBuffer.SetData(deviceContext, data);
	}
	else
	{
		VideoTagData2D data[maxTagDataSize];
		for (int i = 0; i < videoTagData2DArray.size(); ++i)
		{
			const auto& td = videoTagData2DArray[i];
			const auto& pos = td.cameraTransform.position;
			const auto& rot = td.cameraTransform.rotation;

			data[i].cameraPosition = { pos.x, pos.y, pos.z };
			data[i].cameraRotation = { rot.x, rot.y, rot.z, rot.w };
		}
		tagData2DBuffer.SetData(deviceContext, data);
	}
}

void ClientRenderer::DrawOSD(simul::crossplatform::GraphicsDeviceContext& deviceContext)
{
	vec4 white(1.f, 1.f, 1.f, 1.f);
	const avs::NetworkSourceCounters counters = source.getCounterValues();
	//ImGui::Text("Frame #: %d", renderStats.frameCounter);
	//ImGui::PlotLines("FPS", statFPS.data(), statFPS.count(), 0, nullptr, 0.0f, 60.0f);
	deviceContext.framePrintX = 8;
	deviceContext.framePrintY = 8;
	renderPlatform->LinePrint(deviceContext,sessionClient.IsConnected()? simul::base::QuickFormat("Client %d connected to: %s, port %d"
		, sessionClient.GetClientID(),sessionClient.GetServerIP().c_str(),sessionClient.GetPort()):
		(canConnect?simul::base::QuickFormat("Not connected. Discovering on port %d",REMOTEPLAY_CLIENT_DISCOVERY_PORT):"Offline"),white);
	renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Framerate: %4.4f", framerate));
	if(show_osd== NETWORK_OSD)
	{
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Start timestamp: %d", pipeline.GetStartTimestamp()));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Current timestamp: %d",pipeline.GetTimestamp()));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Bandwidth: %4.4f", counters.bandwidthKPS));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Jitter Buffer Length: %d ", counters.jitterBufferLength ));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Jitter Buffer Push: %d ", counters.jitterBufferPush));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Jitter Buffer Pop: %d ", counters.jitterBufferPop )); 
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Network packets received: %d", counters.networkPacketsReceived));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Network Packet orphans: %d", counters.m_packetMapOrphans));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Max age: %d", counters.m_maxAge));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Decoder packets received: %d", counters.decoderPacketsReceived));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Network packets dropped: %d", counters.networkPacketsDropped));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Decoder packets dropped: %d", counters.decoderPacketsDropped)); 
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Decoder packets incomplete: %d", counters.incompleteDPsReceived));
	}
	else if(show_osd== CAMERA_OSD)
	{
		vec3 viewpos=camera.GetPosition();
		renderPlatform->LinePrint(deviceContext, receivedInitialPos?(simul::base::QuickFormat("Origin: %4.4f %4.4f %4.4f", oculusOrigin.x, oculusOrigin.y, oculusOrigin.z)):"Origin:", white);
		renderPlatform->LinePrint(deviceContext,  simul::base::QuickFormat("  View: %4.4f %4.4f %4.4f", viewpos.x, viewpos.y, viewpos.z),white);
		if (videoPosDecoded)
		{
			renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat(" Video: %4.4f %4.4f %4.4f", videoPos.x, videoPos.y, videoPos.z), white);
		}	
		else
		{
			renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat(" Video: -"), white);
		}
	}
	else if(show_osd==GEOMETRY_OSD)
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Actors: %d",resourceManagers.mActorManager->GetActorAmount()), white);
		auto &rootActors=resourceManagers.mActorManager->GetRootActors();
		for(auto &a:rootActors)
		{
			renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("\t%d ",a->id));
		}
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Meshes: %d\nLights: %d"	,resourceManagers.mMeshManager.GetCache(cacheLock).size()
																									,resourceManagers.mLightManager.GetCache(cacheLock).size()), white);
		auto &cachedLights=resourceManagers.mLightManager.GetCache(cacheLock);

		int j=0;
		for(auto &i:cachedLights)
		{
			auto &l=i.second;
			if(l.resource)
			{
				auto &L=l.resource->GetLightData();
				if(L.is_point==0.0f)
					renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("\t%d: %3.3f %3.3f %3.3f, dir %3.3f %3.3f %3.3f",i.first,L.colour.x,L.colour.y,L.colour.z,L.direction.x,L.direction.y,L.direction.z));
				else
					renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("\t%d: %3.3f %3.3f %3.3f, pos %3.3f %3.3f %3.3f, rad %3.3f",i.first,L.colour.x,L.colour.y,L.colour.z,L.position.x,L.position.y,L.position.z,L.radius));
			}
			if(j<videoTagDataCubeArray[0].lights.size())
			{
				auto &l=videoTagDataCubeArray[0].lights[j];
				renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("\t shadow orig %3.3f %3.3f %3.3f",l.position.x,l.position.y,l.position.z));
			}
			j++;
		}
		
		
		auto &missing=resourceCreator.GetMissingResources();
		if(missing.size())
		{
			renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("Missing Resources"));
			for(auto m:missing)
			{
				renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("\t%d",m.first));
			}
		}
	}
	else if(show_osd== CONTROLLER_OSD)
	{
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("     Mouse: %d %d %3.3d",mouseCameraInput.MouseX,mouseCameraInput.MouseY,mouseCameraState.right_left_spd));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("      btns: %d",mouseCameraInput.MouseButtons));
		
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("   view_dir: %3.3f %3.3f %3.3f", controllerSim.view_dir.x, controllerSim.view_dir.y, controllerSim.view_dir.z));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("        dir: %3.3f %3.3f %3.3f", controllerSim.controller_dir.x, controllerSim.controller_dir.y, controllerSim.controller_dir.z));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("      angle: %3.3f", controllerSim.angle));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat(" pos_offset: %3.3f %3.3f %3.3f", controllerSim.pos_offset[0].x, controllerSim.pos_offset[0].y, controllerSim.pos_offset[0].z));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("           : %3.3f %3.3f %3.3f", controllerSim.pos_offset[1].x, controllerSim.pos_offset[1].y, controllerSim.pos_offset[1].z));

		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("   position: %3.3f %3.3f %3.3f", controllerSim.position[0].x, controllerSim.position[0].y, controllerSim.position[0].z));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("           : %3.3f %3.3f %3.3f", controllerSim.position[1].x, controllerSim.position[1].y, controllerSim.position[1].z));

		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("orientation: %3.3f %3.3f %3.3f", controllerSim.orientation[0].x, controllerSim.orientation[0].y, controllerSim.orientation[0].z, controllerSim.orientation[0].w));
		renderPlatform->LinePrint(deviceContext, simul::base::QuickFormat("           : %3.3f %3.3f %3.3f", controllerSim.orientation[1].x, controllerSim.orientation[1].y, controllerSim.orientation[1].z, controllerSim.orientation[1].w));

	}

	//ImGui::PlotLines("Jitter buffer length", statJitterBuffer.data(), statJitterBuffer.count(), 0, nullptr, 0.0f, 100.0f);
	//ImGui::PlotLines("Jitter buffer push calls", statJitterPush.data(), statJitterPush.count(), 0, nullptr, 0.0f, 5.0f);
	//ImGui::PlotLines("Jitter buffer pop calls", statJitterPop.data(), statJitterPop.count(), 0, nullptr, 0.0f, 5.0f);
	PrintHelpText(deviceContext);
}

void ClientRenderer::WriteHierarchy(int tab,std::shared_ptr<scr::Node> actor)
{
	for(int i=0;i<tab;i++)
		std::cout<<"\t";
	std::cout<<actor->id<<std::endl;
	for(auto a:actor->GetChildren())
	{
		WriteHierarchy(tab+1,a.lock());
	}
}

void ClientRenderer::WriteHierarchies()
{
	for(std::shared_ptr<scr::Node> actor : resourceManagers.mActorManager->GetRootActors())
	{
		WriteHierarchy(0,actor);
	}
}
void ClientRenderer::RenderLocalActors(simul::crossplatform::GraphicsDeviceContext& deviceContext)
{
	deviceContext.viewStruct.view = camera.MakeViewMatrix();
	deviceContext.viewStruct.Init();

	cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
	cameraConstants.view = deviceContext.viewStruct.view;
	cameraConstants.proj = deviceContext.viewStruct.proj;
	cameraConstants.viewProj = deviceContext.viewStruct.viewProj;
	cameraConstants.viewPosition = camera.GetPosition();

	const scr::ActorManager::actorList_t& actorList = resourceManagers.mActorManager->GetRootActors();
	for(std::shared_ptr<scr::Node> actor : resourceManagers.mActorManager->GetRootActors())
	{
		RenderActor(deviceContext, actor);
	}
}

void ClientRenderer::RenderActor(simul::crossplatform::GraphicsDeviceContext& deviceContext, std::shared_ptr<scr::Node> actor)
{
	AVSTextureHandle th = avsTextures[0];
	AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto &cachedLights=resourceManagers.mLightManager.GetCache(cacheLock);
		if(cachedLights.size()>lightsBuffer.count)
		{
			lightsBuffer.InvalidateDeviceObjects();
			lightsBuffer.RestoreDeviceObjects(renderPlatform,cachedLights.size());
		}
		pbrConstants.lightCount=cachedLights.size();
	}
	PbrLight *l=(PbrLight*)(const_cast<scr::Light::LightData*>(scr::Light::GetAllLightData().data()));
	lightsBuffer.SetData(deviceContext,l);

	//Only render visible actors, but still render children that are close enough.
	if(actor->IsVisible())
	{
		const scr::Transform& transform = actor->GetGlobalTransform();
		const std::shared_ptr<scr::Mesh> mesh = actor->GetMesh();
		if(mesh)
		{
			const auto& meshInfo = mesh->GetMeshCreateInfo();
			static int mat_select=-1;
			for(size_t element = 0; element < actor->GetMaterials().size() && element < meshInfo.ib.size(); element++)
			{
				if(mat_select >= 0 && mat_select != element) continue;

				auto* vb = dynamic_cast<pc_client::PC_VertexBuffer*>(meshInfo.vb[element].get());
				const auto* ib = dynamic_cast<pc_client::PC_IndexBuffer*>(meshInfo.ib[element].get());

				const simul::crossplatform::Buffer* const v[] = {vb->GetSimulVertexBuffer()};
				simul::crossplatform::Layout* layout = vb->GetLayout();

				mat4 model;
				model = ((const float*)&(transform.GetTransformMatrix()));
				mat4::mul(cameraConstants.worldViewProj, *((mat4*)&deviceContext.viewStruct.viewProj), model);
				cameraConstants.world = model;

				std::shared_ptr<scr::Material> material = actor->GetMaterials()[element];
				if(!material)
				{
					//Actor incomplete.
					continue;
				}
				else
				{
					const scr::Material::MaterialCreateInfo& matInfo = material->GetMaterialCreateInfo();
					const scr::Material::MaterialData& md = material->GetMaterialData();
					memcpy(&pbrConstants.diffuseOutputScalar, &md, sizeof(md));

					std::shared_ptr<pc_client::PC_Texture> diffuse = std::dynamic_pointer_cast<pc_client::PC_Texture>(matInfo.diffuse.texture);
					std::shared_ptr<pc_client::PC_Texture> normal = std::dynamic_pointer_cast<pc_client::PC_Texture>(matInfo.normal.texture);
					std::shared_ptr<pc_client::PC_Texture> combined = std::dynamic_pointer_cast<pc_client::PC_Texture>(matInfo.combined.texture);
					std::shared_ptr<pc_client::PC_Texture> emissive = std::dynamic_pointer_cast<pc_client::PC_Texture>(matInfo.emissive.texture);

					pbrEffect->SetTexture(deviceContext, pbrEffect->GetShaderResource("diffuseTexture"), diffuse ? diffuse->GetSimulTexture() : nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect->GetShaderResource("normalTexture"), normal ? normal->GetSimulTexture() : nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect->GetShaderResource("combinedTexture"), combined ? combined->GetSimulTexture() : nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect->GetShaderResource("emissiveTexture"), emissive ? emissive->GetSimulTexture() : nullptr);

					pbrEffect->SetTexture(deviceContext, "specularCubemap", specularCubemapTexture);
					pbrEffect->SetTexture(deviceContext, "roughSpecularCubemap", roughSpecularCubemapTexture);
					pbrEffect->SetTexture(deviceContext, "diffuseCubemap", diffuseCubemapTexture);
					pbrEffect->SetTexture(deviceContext, "videoTexture", ti->texture);
				//	pbrEffect->SetTexture(deviceContext, "lightingCubemap", lightingCubemapTexture);
				}
				
				lightsBuffer.Apply(deviceContext, pbrEffect, _lights );
				tagDataCubeBuffer.Apply(deviceContext, pbrEffect, pbrEffect->GetShaderResource("TagDataCubeBuffer"));
				tagDataIDBuffer.Apply(deviceContext, pbrEffect, pbrEffect->GetShaderResource("TagDataIDBuffer"));
				std::string usedPassName = passName;

				std::shared_ptr<scr::Skin> skin = actor->GetSkin();
				if(skin)
				{
					std::vector<scr::mat4> scr_matrices = skin->GetBoneMatrices();
					memcpy(&boneMatrices.boneMatrices, scr_matrices.data(), sizeof(scr::mat4) * 64);

					pbrEffect->SetConstantBuffer(deviceContext, &boneMatrices);
					usedPassName = "anim_" + usedPassName;
				}

				pbrEffect->SetConstantBuffer(deviceContext, &pbrConstants);
				pbrEffect->SetConstantBuffer(deviceContext, &cameraConstants);
				renderPlatform->SetLayout(deviceContext, layout);
				renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLELIST);
				renderPlatform->SetVertexBuffers(deviceContext, 0, 1, v, layout);
				renderPlatform->SetIndexBuffer(deviceContext, ib->GetSimulIndexBuffer());
				pbrEffect->Apply(deviceContext, pbrEffect->GetTechniqueByName("solid"), usedPassName.c_str());
				renderPlatform->DrawIndexed(deviceContext, (int)ib->GetIndexBufferCreateInfo().indexCount, 0, 0);
				pbrEffect->Unapply(deviceContext);
				layout->Unapply(deviceContext);
			}
		}
	}

	for(std::weak_ptr<scr::Node> childPtr : actor->GetChildren())
	{
		std::shared_ptr<scr::Node> child = childPtr.lock();
		if(child)
		{
			RenderActor(deviceContext, child);
		}
	}
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
	SAFE_DELETE(specularCubemapTexture);
	SAFE_DELETE(roughSpecularCubemapTexture);
	SAFE_DELETE(lightingCubemapTexture);
	SAFE_DELETE(videoTexture);
	SAFE_DELETE(meshRenderer);
	SAFE_DELETE(hDRRenderer);
	SAFE_DELETE(hdrFramebuffer);
	SAFE_DELETE(pbrEffect);
	SAFE_DELETE(cubemapClearEffect);
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
	float timeElapsed = (timestamp - previousTimestamp) / 1000.0f;//ns to ms

	resourceManagers.Update(timeElapsed);
	resourceCreator.Update(timeElapsed);

	previousTimestamp = timestamp;
}

void ClientRenderer::OnVideoStreamChanged(const char *server_ip,const avs::SetupCommand &setupCommand,avs::Handshake &handshake)
{
	videoConfig = setupCommand.video_config;

	WARN("VIDEO STREAM CHANGED: port %d clr %d x %d dpth %d x %d", setupCommand.port, videoConfig.video_width, videoConfig.video_height
																	, videoConfig.depth_width, videoConfig.depth_height	);
	videoPosDecoded=false;

	videoTagData2DArray.clear();
	videoTagData2DArray.resize(maxTagDataSize);
	videoTagDataCubeArray.clear();
	videoTagDataCubeArray.resize(maxTagDataSize);

	sourceParams.nominalJitterBufferLength = NominalJitterBufferLength;
	sourceParams.maxJitterBufferLength = MaxJitterBufferLength;
	sourceParams.socketBufferSize = 1212992;
	sourceParams.requiredLatencyMs=setupCommand.requiredLatencyMs;
	auto numStreams = NumVidStreams + (AudioStream ? 1 : 0) + (GeoStream ? 1 : 0);
	// Configure for num video streams + 1 audio stream + 1 geometry stream
	if (!source.configure(numStreams, setupCommand.port+1, server_ip, setupCommand.port, sourceParams))
	{
		LOG("Failed to configure network source node");
		return;
	}
	source.setDebugStream(setupCommand.debug_stream);
	source.setDoChecksums(setupCommand.do_checksums);
	source.setDebugNetworkPackets(setupCommand.debug_network_packets);
	
	decoderParams.deferDisplay = false;
	decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
	decoderParams.codec = videoConfig.videoCodec;
	decoderParams.use10BitDecoding = videoConfig.use_10_bit_decoding;
	decoderParams.useYUV444ChromaFormat = videoConfig.use_yuv_444_decoding;

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
	size_t stream_width = videoConfig.video_width;
	size_t stream_height = videoConfig.video_height;

	if (videoConfig.use_cubemap)
	{
		videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, videoConfig.colour_cubemap_size, videoConfig.colour_cubemap_size, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, true);
		specularCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, videoConfig.specular_cubemap_size, videoConfig.specular_cubemap_size, 1, 3, crossplatform::PixelFormat::RGBA_8_UNORM, true, false, true);
		roughSpecularCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, videoConfig.rough_cubemap_size, videoConfig.rough_cubemap_size, 1, 3, crossplatform::PixelFormat::RGBA_8_UNORM, true, false, true);
		lightingCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, videoConfig.light_cubemap_size, videoConfig.light_cubemap_size, 1, 1,
			crossplatform::PixelFormat::RGBA_8_UNORM, true, false, true);
		diffuseCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, videoConfig.diffuse_cubemap_size, videoConfig.diffuse_cubemap_size, 1, 1,
			crossplatform::PixelFormat::RGBA_8_UNORM, true, false, true);
	}
	else
	{
		videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, 1920, 1080, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, false);
	}
	
	colourOffsetScale.x = 0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(videoConfig.video_height) / float(stream_height);

	for (int i = 0; i < NumVidStreams; ++i)
	{
		CreateTexture(avsTextures[i], int(stream_width),int(stream_height), SurfaceFormats[i]);
		auto f = std::bind(&ClientRenderer::OnReceiveVideoTagData, this, std::placeholders::_1, std::placeholders::_2);
		// Video streams are 0+...
		if (!decoder[i].configure(dev, (int)stream_width, (int)stream_height, decoderParams, 20 + i, f))
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

	// Audio
	if (AudioStream)
	{
		avsAudioDecoder.configure(40);
		sca::AudioParams audioParams;
		audioParams.codec = sca::AudioCodec::PCM;
		audioParams.numChannels = 2;
		audioParams.sampleRate = 48000;
		audioParams.bitsPerSample = 32;
		// This will be deconfigured automatically when the pipeline is deconfigured.
		audioPlayer.configure(audioParams);
		audioStreamTarget.reset(new sca::AudioStreamTarget(&audioPlayer));
		avsAudioTarget.configure(audioStreamTarget.get());
		pipeline.link({ &source, &avsAudioDecoder, &avsAudioTarget });
	}

	// We will add a GEOMETRY PIPE
	if(GeoStream)
	{
		avsGeometryDecoder.configure(60,&geometryDecoder);
		avsGeometryTarget.configure(&resourceCreator);
		pipeline.link({ &source, &avsGeometryDecoder, &avsGeometryTarget });
	}

	handshake.startDisplayInfo.width = hdrFramebuffer->GetWidth();
	handshake.startDisplayInfo.height = hdrFramebuffer->GetHeight();
	handshake.axesStandard = avs::AxesStandard::EngineeringStyle;
	handshake.MetresPerUnit = 1.0f;
	handshake.FOV = 90.0f;
	handshake.isVR = false;
	handshake.framerate = 60;
	handshake.udpBufferSize = static_cast<uint32_t>(source.getSystemBufferSize());
	handshake.maxBandwidthKpS = handshake.udpBufferSize * handshake.framerate;

	lastSetupCommand = setupCommand;
	//java->Env->CallVoidMethod(java->ActivityObject, jni.initializeVideoStreamMethod, port, width, height, mVideoSurfaceTexture->GetJavaObject());
}

void ClientRenderer::OnVideoStreamClosed()
{
	WARN("VIDEO STREAM CLOSED");
	pipeline.deconfigure();
	//const ovrJava* java = app->GetJava();
	//java->Env->CallVoidMethod(java->ActivityObject, jni.closeVideoStreamMethod);

	receivedInitialPos = false;
}

void ClientRenderer::OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand)
{
	videoConfig = reconfigureVideoCommand.video_config;

	WARN("VIDEO STREAM RECONFIGURED: clr %d x %d dpth %d x %d", videoConfig.video_width, videoConfig.video_height
		, videoConfig.depth_width, videoConfig.depth_height);

	decoderParams.deferDisplay = false;
	decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
	decoderParams.codec = videoConfig.videoCodec;
	decoderParams.use10BitDecoding = videoConfig.use_10_bit_decoding;
	decoderParams.useYUV444ChromaFormat = videoConfig.use_yuv_444_decoding;

	avs::DeviceHandle dev;
	dev.handle = renderPlatform->AsD3D11Device();
	dev.type = avs::DeviceType::Direct3D11;

	size_t stream_width = videoConfig.video_width;
	size_t stream_height = videoConfig.video_height;

	if (videoConfig.use_cubemap)
	{
		videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, videoConfig.colour_cubemap_size, videoConfig.colour_cubemap_size, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, true);
	}
	else
	{
		videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, stream_width, stream_height, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, false);
	}

	colourOffsetScale.x = 0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(videoConfig.video_height) / float(stream_height);

	for (size_t i = 0; i < NumVidStreams; ++i)
	{
		AVSTextureImpl* ti = (AVSTextureImpl*)(avsTextures[i].get());
		// Only create new texture and register new surface if resolution has changed
		if (ti && ti->texture->GetWidth() != stream_width || ti->texture->GetLength() != stream_height)
		{
			SAFE_DELETE(ti->texture);

			if (!decoder[i].unregisterSurface())
			{
				throw std::runtime_error("Failed to unregister decoder surface");
			}

			CreateTexture(avsTextures[i], int(stream_width), int(stream_height), SurfaceFormats[i]);
		}

		if (!decoder[i].reconfigure((int)stream_width, (int)stream_height, decoderParams))
		{
			throw std::runtime_error("Failed to reconfigure decoder");
		}
	}
	lastSetupCommand.video_config = reconfigureVideoCommand.video_config;
}

void ClientRenderer::OnReceiveVideoTagData(const uint8_t* data, size_t dataSize)
{
	if (lastSetupCommand.video_config.use_cubemap)
	{
		scr::SceneCaptureCubeTagData tagData;
		memcpy(&tagData.coreData, data, sizeof(scr::SceneCaptureCubeCoreTagData));
		avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, tagData.coreData.cameraTransform);

		tagData.lights.resize(tagData.coreData.lightCount);

		// Aidan : View and proj matrices are currently unchanged from Unity
		size_t index = sizeof(scr::SceneCaptureCubeCoreTagData);
		for (auto& light : tagData.lights)
		{
			memcpy(&light, &data[index], sizeof(scr::LightData));
			avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, light.worldTransform);

			index += sizeof(scr::LightData);
		}
		videoTagDataCubeArray[tagData.coreData.id] = std::move(tagData);
	}
	else
	{
		scr::SceneCapture2DTagData tagData;
		memcpy(&tagData, data, dataSize);
		avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, tagData.cameraTransform);
		videoTagData2DArray[tagData.id] = std::move(tagData);
	}
}

bool ClientRenderer::OnActorEnteredBounds(avs::uid actor_uid)
{
	return resourceManagers.mActorManager->ShowActor(actor_uid);
}

bool ClientRenderer::OnActorLeftBounds(avs::uid actor_uid)
{
	return resourceManagers.mActorManager->HideActor(actor_uid);
}

std::vector<uid> ClientRenderer::GetGeometryResources()
{
	return resourceManagers.GetAllResourceIDs();
}

void ClientRenderer::ClearGeometryResources()
{
	resourceManagers.Clear();
}

void ClientRenderer::SetVisibleActors(const std::vector<avs::uid>& visibleActors)
{
	resourceManagers.mActorManager->SetVisibleActors(visibleActors);
}

void ClientRenderer::UpdateActorMovement(const std::vector<avs::MovementUpdate>& updateList)
{
	resourceManagers.mActorManager->UpdateActorMovement(updateList);
}
#include "Platform/CrossPlatform/Quaterniond.h"
void ClientRenderer::FillInControllerPose(int index,avs::Pose& pose, float offset)
{
	if(!hdrFramebuffer->GetHeight())
		return;
	float x= mouseCameraInput.MouseX / (float)hdrFramebuffer->GetWidth();
	float y= mouseCameraInput.MouseY / (float)hdrFramebuffer->GetHeight();
	controllerSim.controller_dir=camera.ScreenPositionToDirection(x,y, hdrFramebuffer->GetWidth()/ hdrFramebuffer->GetHeight());
	controllerSim.view_dir=camera.ScreenPositionToDirection(0.5f,0.5f,1.0f);
	// we seek the angle positive on the Z-axis representing the view direction azimuth:
	controllerSim.angle=atan2f(-controllerSim.view_dir.x, controllerSim.view_dir.y);
	float sine= sin(controllerSim.angle), cosine=cos(controllerSim.angle);
	static float hand_dist=0.5f;
	controllerSim.pos_offset[index]=vec3(hand_dist*(-sine+offset*cosine),hand_dist*(cosine+offset*sine),0.0f);
	// Get horizontal azimuth of view.
	pose.position=camera.GetPosition();
	pose.position+=*((avs::vec3*)&controllerSim.pos_offset[index]);

	// For the orientation, we want to point the controller towards controller_dir. The pointing direction is y.
	// The up direction is x, and the left direction is z.
	simul::crossplatform::Quaternion<float> q(0,0,0,1.0f);
	float azimuth= atan2f(-controllerSim.controller_dir.x, controllerSim.controller_dir.y);
	float elevation=asin(controllerSim.controller_dir.z);
	q.Rotate(azimuth,vec3(0,0,1.0f));
	q.Rotate(elevation, vec3(1.0f, 0, 0));

	controllerSim.position[index] =pose.position;
	controllerSim.orientation[index] =((const float*)&q);
	pose.orientation= ((const float*)&q);
}

void ClientRenderer::OnFrameMove(double fTime,float time_step)
{
	mouseCameraInput.forward_back_input	=(float)keydown['w']-(float)keydown['s'];
	mouseCameraInput.right_left_input	=(float)keydown['d']-(float)keydown['a'];
	mouseCameraInput.up_down_input		=(float)keydown['q']-(float)keydown['z'];
	static float spd = 2.0f;
	crossplatform::UpdateMouseCamera(&camera
							,time_step
							,spd
							,mouseCameraState
							,mouseCameraInput
							,14000.f);
	controllerStates[0].mTrackpadX=0.5f;
	controllerStates[0].mTrackpadY=0.5f;
	controllerStates[0].mJoystickAxisX	=mouseCameraInput.right_left_input;
	controllerStates[0].mJoystickAxisY	=mouseCameraInput.forward_back_input;
	controllerStates[0].mButtons		=mouseCameraInput.MouseButtons;

	// Reset
	mouseCameraInput.MouseButtons = 0;
	controllerStates[0].mTrackpadStatus=true;
	// Handle networked session.
	if (sessionClient.IsConnected())
	{
		vec3 forward=-camera.Orientation.Tz();
		// std::cout << forward.x << " " << forward.y << " " << forward.z << "\n";
		// The camera has Z backward, X right, Y up.
		// But we want orientation relative to X right, Y forward, Z up.
		simul::math::Quaternion q0(3.1415926536f/2.0f, simul::math::Vector3(1.f, 0.0f, 0.0f));

		if(!receivedInitialPos)
		{
			camera.SetPositionAsXYZ(0.f, 0.f, 5.f);
			vec3 look(0.f, 1.f, 0.f), up(0.f, 0.f, 1.f);
			camera.LookInDirection(look, up);
		}
		auto q = camera.Orientation.GetQuaternion();
		auto q_rel=q/q0;
		avs::DisplayInfo displayInfo = {static_cast<uint32_t>(hdrFramebuffer->GetWidth()), static_cast<uint32_t>(hdrFramebuffer->GetHeight())};
		avs::Pose headPose;
		headPose.orientation = *((avs::vec4*) & q_rel);
		vec3 pos = camera.GetPosition();
		headPose.position = *((avs::vec3*) & pos);
		
		avs::Pose controllerPoses[2];
		FillInControllerPose(0,controllerPoses[0],1.0f);
		FillInControllerPose(1,controllerPoses[1], -1.0f);

		sessionClient.Frame(displayInfo, headPose, controllerPoses, receivedInitialPos, controllerStates, decoder->idrRequired(),fTime);
		if (receivedInitialPos!=sessionClient.receivedInitialPos&& sessionClient.receivedInitialPos>0)
		{
			oculusOrigin = sessionClient.GetInitialPos();
			vec3 pos = (const float*)& oculusOrigin;
			camera.SetPosition(pos);
			receivedInitialPos = sessionClient.receivedInitialPos;
		}
		avs::Result result = pipeline.process();

		static short c = 0;
		if (!(c--))
		{
			const avs::NetworkSourceCounters Counters = source.getCounterValues();
			std::cout << "Network packets dropped: " << 100.0f*Counters.networkDropped << "%"
				<< "\nDecoder packets dropped: " << 100.0f*Counters.decoderDropped << "%"
				<< std::endl;
		}
	}
	else
	{
		ENetAddress remoteEndpoint; //192.168.3.42 45.132.108.84
		if (canConnect && sessionClient.Discover("", REMOTEPLAY_CLIENT_DISCOVERY_PORT, server_ip.c_str(), server_discovery_port, remoteEndpoint))
		{
			sessionClient.Connect(remoteEndpoint, REMOTEPLAY_TIMEOUT);
		}
	}

	//sessionClient.Frame(camera.GetOrientation().GetQuaternion(), controllerState);
}

void ClientRenderer::OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta)
{
}

void ClientRenderer::OnMouseButtonClicked(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta)
{
	mouseCameraInput.MouseButtons
		|= (bLeftButtonDown ? crossplatform::MouseCameraInput::LEFT_BUTTON : 0)
		| (bRightButtonDown ? crossplatform::MouseCameraInput::RIGHT_BUTTON : 0)
		| (bMiddleButtonDown ? crossplatform::MouseCameraInput::MIDDLE_BUTTON : 0);
}

void ClientRenderer::OnMouseMove(int xPos
			,int yPos,bool bLeftButtonDown
			,bool bRightButtonDown
			,bool bMiddleButtonDown
			,int nMouseWheelDelta
			 )
{
	mouseCameraInput.MouseX=xPos;
	mouseCameraInput.MouseY=yPos;
	mouseCameraInput.MouseButtons
		|= (bLeftButtonDown ? crossplatform::MouseCameraInput::LEFT_BUTTON : 0)
		| (bRightButtonDown ? crossplatform::MouseCameraInput::RIGHT_BUTTON : 0)
		| (bMiddleButtonDown ? crossplatform::MouseCameraInput::MIDDLE_BUTTON : 0);
}

void ClientRenderer::PrintHelpText(simul::crossplatform::GraphicsDeviceContext& deviceContext)
{
	deviceContext.framePrintY = 8;
	deviceContext.framePrintX = hdrFramebuffer->GetWidth() / 2;
	renderPlatform->LinePrint(deviceContext, "K: Connect/Disconnect");
	renderPlatform->LinePrint(deviceContext, "O: Toggle OSD");
	renderPlatform->LinePrint(deviceContext, "V: Show video");
	renderPlatform->LinePrint(deviceContext, "C: Toggle render from centre");
	renderPlatform->LinePrint(deviceContext, "T: Toggle Textures");
	renderPlatform->LinePrint(deviceContext, "M: Change rendermode");
	renderPlatform->LinePrint(deviceContext, "R: Recompile shaders");
	renderPlatform->LinePrint(deviceContext, "NUM 0: PBR");
	renderPlatform->LinePrint(deviceContext, "NUM 1: Albedo");
	renderPlatform->LinePrint(deviceContext, "NUM 4: Unswizzled Normals");
	renderPlatform->LinePrint(deviceContext, "NUM 5: Unreal Normals");
	renderPlatform->LinePrint(deviceContext, "NUM 6: Unity Normals");
	renderPlatform->LinePrint(deviceContext, "NUM 2: Vertex Normals");
}


void ClientRenderer::OnKeyboard(unsigned wParam,bool bKeyDown)
{
	switch (wParam) 
	{
		case VK_LEFT: 
		case VK_RIGHT: 
		case VK_UP: 
		case VK_DOWN:
			return;
		default:
			int  k = tolower(wParam);
			if (k > 255)
				return;
			keydown[k] = bKeyDown ? 1 : 0;
		break; 
	}
	if (!bKeyDown)
	{
		switch (wParam)
		{
		case 'V':
			show_video = !show_video;
			break;
		case 'O':
			show_osd =(show_osd+1)%NUM_OSDS;
			break;
		case 'C':
			render_from_video_centre = !render_from_video_centre;
			break;
		case 'H':
			WriteHierarchies();
			break;
		case 'T':
			show_textures = !show_textures;
			break;
		case 'K':
			sessionClient.Disconnect(0);
			canConnect=!canConnect;
			break;
		case 'M':
			RenderMode++;
			RenderMode = RenderMode % 2;
			break;
		case 'R':
			RecompileShaders();
			break;
		case VK_NUMPAD0: //Display full PBR rendering.
			ChangePass(ShaderMode::PBR);
			break;
		case VK_NUMPAD1: //Display only albedo/diffuse.
			ChangePass(ShaderMode::ALBEDO);
			break;
		case VK_NUMPAD4: //Display normals for native PC client frame-of-reference.
			ChangePass(ShaderMode::NORMAL_UNSWIZZLED);
			break;
		case VK_NUMPAD5: //Display normals swizzled for matching Unreal output.
			ChangePass(ShaderMode::NORMAL_UNREAL);
			break;
		case VK_NUMPAD6: //Display normals swizzled for matching Unity output.
			ChangePass(ShaderMode::NORMAL_UNITY);
			break;
		case VK_NUMPAD2: //Display normals swizzled for matching Unity output.
			ChangePass(ShaderMode::NORMAL_VERTEXNORMALS);
			break;
		default:
			break;
		}
	}
}
