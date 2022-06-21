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
		protected:
			avs::DecoderBackendInterface* CreateVideoDecoder() override;
		};
	}
}