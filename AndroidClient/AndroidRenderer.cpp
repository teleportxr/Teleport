#include "AndroidRenderer.h"
#include "ClientRender/VideoDecoderBackend.h"
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

AndroidRenderer::AndroidRenderer(teleport::client::ClientDeviceState *clientDeviceState,teleport::client::SessionClient *s,teleport::Gui &g,teleport::client::Config &cfg)
	:clientrender::Renderer(clientDeviceState,new clientrender::NodeManager,new clientrender::NodeManager,s,g,cfg)
{
}

AndroidRenderer::~AndroidRenderer()
{
}

void AndroidRenderer::OnFrameAvailable()
{
}

avs::DecoderBackendInterface* AndroidRenderer::CreateVideoDecoder()
{
	clientrender::AVSTextureHandle th = avsTexture;
	AVSTextureImpl* t = static_cast<AVSTextureImpl*>(th.get());
	return new VideoDecoderBackend(renderPlatform,t->texture,this);
}