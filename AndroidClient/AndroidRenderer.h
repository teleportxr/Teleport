#include "ClientRender/Renderer.h"
#include "TeleportClient/ClientDeviceState.h"
#include "VideoDecoderBackend.h"

namespace teleport
{
	namespace android
	{
		class AndroidRenderer : public clientrender::Renderer,public DecodeEventInterface
		{
		public:
			AndroidRenderer(teleport::client::ClientDeviceState *clientDeviceState,teleport::client::SessionClient *s,teleport::Gui &g,teleport::client::Config &cfg);
			~AndroidRenderer();
			void OnFrameAvailable() override;
			void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext ) override;
		protected:
			avs::DecoderBackendInterface* CreateVideoDecoder() override;
			avs::DecoderStatus GetVideoDecoderStatus() override;
			VideoDecoderBackend *videoDecoderBackend= nullptr;
		};
	}
}