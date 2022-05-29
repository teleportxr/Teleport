#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#ifdef _MSC_VER
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
#include "Platform/CrossPlatform/SphericalHarmonics.h"
#include "Platform/CrossPlatform/Quaterniond.h"

#include "Config.h"
#include "PCDiscoveryService.h"

#include <algorithm>
#include <random>
#include <functional>

#if TELEPORT_CLIENT_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include <libavstream/surfaces/surface_dx12.hpp>
#endif
#if TELEPORT_CLIENT_USE_D3D11
#include <libavstream/surfaces/surface_dx11.hpp>
#endif
#ifdef _MSC_VER
#include "libavstream/platforms/platform_windows.hpp"
#endif
#ifdef __ANDROID__
#include "libavstream/platforms/platform_posix.hpp"
#endif
#include "ClientRender/Light.h"
#include "ClientRender/Material.h"
#include "TeleportClient/SessionClient.h"
#include "ClientRender/Tests.h"
#include "ClientRender/Renderer.h"

#include "TeleportClient/ServerTimestamp.h"


#include "VideoDecoder.h"

#include "TeleportCore/ErrorHandling.h"

using namespace teleport;


std::default_random_engine generator;
std::uniform_real_distribution<float> rando(-1.0f,1.f);

using namespace platform;

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
#ifdef _MSC_VER
#include <windows.h>
#include <SDKDDKVer.h>
#include <shellapi.h>
#endif
#include "ClientRenderer.h"
using namespace teleport;
using namespace client;

struct AVSTextureImpl :public clientrender::AVSTexture
{
	AVSTextureImpl(platform::crossplatform::Texture *t)
		:texture(t)
	{
	}
	platform::crossplatform::Texture *texture = nullptr;
	avs::SurfaceBackendInterface* createSurface() const override
	{
#if TELEPORT_CLIENT_USE_D3D12
		return new avs::SurfaceDX12(texture->AsD3D12Resource());
#endif
#if TELEPORT_CLIENT_USE_D3D11
		return new avs::SurfaceDX11(texture->AsD3D11Texture2D());
#endif
#if TELEPORT_CLIENT_USE_VULKAN
		return new avs::SurfaceVulkan(texture->AsD3D11Texture2D());
#endif
	}
};


ClientRenderer::ClientRenderer(teleport::client::ClientDeviceState *c,teleport::client::SessionClient *sc, teleport::Gui& g,bool dev):
	Renderer(c,new clientrender::NodeManager,new clientrender::NodeManager,sc,g,dev)
{
}

ClientRenderer::~ClientRenderer()
{
}

#endif
