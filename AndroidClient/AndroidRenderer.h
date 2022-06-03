#include "ClientRender/Renderer.h"
#include "TeleportClient/ClientDeviceState.h"

namespace teleport
{
	namespace android
	{
		class AndroidRenderer : public clientrender::Renderer
		{
		public:
			AndroidRenderer(teleport::client::ClientDeviceState *clientDeviceState,teleport::client::SessionClient *s,teleport::Gui &g,bool dev_mode);
			~AndroidRenderer();
		};
	}
}