#include "AndroidRenderer.h"
#include "ClientRender/VideoDecoderBackend.h"
#include "ClientRender/InstanceRenderer.h"
#include "VideoDecoderBackend.h"
#include "Platform/Vulkan/Texture.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "libavstream/surfaces/surface_vulkan.hpp"

using namespace teleport;
using namespace android;

struct AVSTextureImpl :public clientrender::AVSTexture
{
	AVSTextureImpl(platform::crossplatform::Texture *t)
		:texture(t)
	{
	}
	platform::crossplatform::Texture *texture = nullptr;
	avs::SurfaceBackendInterface* createSurface() const override
	{
		auto &img=((platform::vulkan::Texture*)texture)->AsVulkanImage();
		return new avs::SurfaceVulkan(&img,texture->width,texture->length,platform::vulkan::RenderPlatform::ToVulkanFormat(texture->pixelFormat));
	}
};

AndroidInstanceRenderer::AndroidInstanceRenderer(avs::uid server,teleport::client::Config &config,GeometryDecoder &geometryDecoder,clientrender::RenderState &renderState,teleport::client::SessionClient *sessionClient)
	:clientrender::InstanceRenderer( server, config,geometryDecoder,renderState,sessionClient)
{
}

AndroidInstanceRenderer::~AndroidInstanceRenderer()
{
}

void AndroidInstanceRenderer::OnFrameAvailable()
{
	//videoDecoderBackend->CopyVideoTexture(deviceContext);
}

avs::DecoderBackendInterface* AndroidInstanceRenderer::CreateVideoDecoder()
{
	clientrender::AVSTextureHandle th = instanceRenderState.avsTexture;
	AVSTextureImpl* t = static_cast<AVSTextureImpl*>(th.get());
	videoDecoderBackend=new VideoDecoderBackend(renderPlatform,t->texture,this);
	return videoDecoderBackend;
}

avs::DecoderStatus AndroidInstanceRenderer::GetVideoDecoderStatus()
{
	avs::DecoderStatus status = avs::DecoderStatus::DecoderUnavailable;
	if (videoDecoderBackend)
		status = videoDecoderBackend->GetDecoderStatus();
	return status;
}

void AndroidInstanceRenderer::RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext)
{
	if(videoDecoderBackend)
		videoDecoderBackend->CopyVideoTexture(deviceContext);
	clientrender::InstanceRenderer::RenderView(deviceContext);
}

AndroidRenderer::AndroidRenderer(teleport::Gui &g,teleport::client::Config &config)
	:Renderer(g,config)
{
}

AndroidRenderer::~AndroidRenderer()
{
}

std::shared_ptr<clientrender::InstanceRenderer> AndroidRenderer::GetInstanceRenderer(avs::uid server_uid)
{
	auto sc=GetSessionClient(server_uid);
	auto i=instanceRenderers.find(server_uid);
	if(i==instanceRenderers.end())
	{
		auto r=std::make_shared<AndroidInstanceRenderer>(server_uid,config,geometryDecoder,renderState,sc.get());
		instanceRenderers[server_uid]=r;
		r->RestoreDeviceObjects(renderPlatform);
		return r;
	}
	return i->second;
}
