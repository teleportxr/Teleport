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

AndroidRenderer::AndroidRenderer(teleport::client::SessionClient *s,teleport::Gui &g,teleport::client::Config &cfg)
	:clientrender::Renderer(s,g,cfg)
{
}

AndroidRenderer::~AndroidRenderer()
{
}

void AndroidRenderer::OnFrameAvailable()
{
	//videoDecoderBackend->CopyVideoTexture(deviceContext);
}

avs::DecoderBackendInterface* AndroidRenderer::CreateVideoDecoder()
{
	clientrender::AVSTextureHandle th = renderState.avsTexture;
	AVSTextureImpl* t = static_cast<AVSTextureImpl*>(th.get());
	videoDecoderBackend=new VideoDecoderBackend(renderPlatform,t->texture,this);
	return videoDecoderBackend;
}

avs::DecoderStatus AndroidRenderer::GetVideoDecoderStatus()
{
	avs::DecoderStatus status = avs::DecoderStatus::DecoderUnavailable;
	if (videoDecoderBackend)
		status = videoDecoderBackend->GetDecoderStatus();
	return status;
}

void AndroidRenderer::RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext)
{
	if(videoDecoderBackend)
		videoDecoderBackend->CopyVideoTexture(deviceContext);
	clientrender::Renderer::RenderView(deviceContext);
}