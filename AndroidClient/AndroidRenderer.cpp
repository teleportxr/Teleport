#include "AndroidRenderer.h"
using namespace teleport;
using namespace android;

AndroidRenderer::AndroidRenderer(teleport::client::ClientDeviceState *clientDeviceState,teleport::client::SessionClient *s,teleport::Gui &g,bool dev)
:clientrender::Renderer(clientDeviceState,new clientrender::NodeManager,new clientrender::NodeManager,s,g,dev)
{
}

AndroidRenderer::~AndroidRenderer()
{
}
