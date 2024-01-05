#include "ClientRender/InstanceRenderer.h"
#include "ClientRender/Renderer.h"
#include "TeleportClient/ClientDeviceState.h"
#include "VideoDecoderBackend.h"

namespace teleport
{
	namespace android
	{
		class AndroidInstanceRenderer : public clientrender::InstanceRenderer,public DecodeEventInterface
		{
		public:
			AndroidInstanceRenderer(avs::uid server,teleport::client::Config &config,clientrender::GeometryDecoder &geometryDecoder,clientrender::RenderState &renderState,teleport::client::SessionClient *sessionClient);
			~AndroidInstanceRenderer();
			void OnFrameAvailable() override;
			void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext ) override;
		protected:
			avs::DecoderBackendInterface* CreateVideoDecoder() override;
			avs::DecoderStatus GetVideoDecoderStatus() override;
			VideoDecoderBackend *videoDecoderBackend= nullptr;
		};
		class AndroidRenderer : public clientrender::Renderer
		{
		public:
			AndroidRenderer(teleport::clientrender::Gui &g);
			virtual ~AndroidRenderer();
		protected:
			std::shared_ptr<clientrender::InstanceRenderer> GetInstanceRenderer(avs::uid server_uid) override;
		};
	}
}