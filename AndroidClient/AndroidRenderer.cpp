#include "AndroidRenderer.h"
#include "ClientRender/VideoDecoder.h"
#include "VideoDecoder.h"
#include "Platform/Vulkan/Texture.h"
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
		return new avs::SurfaceVulkan(&img);
	}
};

AndroidRenderer::AndroidRenderer(teleport::client::ClientDeviceState *clientDeviceState,teleport::client::SessionClient *s,teleport::Gui &g,bool dev)
	:clientrender::Renderer(clientDeviceState,new clientrender::NodeManager,new clientrender::NodeManager,s,g,dev)
{
}

AndroidRenderer::~AndroidRenderer()
{
}

avs::DecoderBackendInterface* AndroidRenderer::CreateVideoDecoder()
{
	clientrender::AVSTextureHandle th = avsTexture;
	AVSTextureImpl* t = static_cast<AVSTextureImpl*>(th.get());
	return new clientrender::VideoDecoder(renderPlatform, t->texture);
}