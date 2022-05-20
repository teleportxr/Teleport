#include "ClientRender/Renderer.h"
#include "TeleportClient/ClientDeviceState.h"

namespace teleport
{
	namespace android
	{
		class AndroidRenderer : public clientrender::Renderer
		{
		public:
			AndroidRenderer(teleport::client::ClientDeviceState *clientDeviceState);
			~AndroidRenderer();
			void ConfigureVideo(const avs::VideoConfig &vc) override;
		};
	}
}