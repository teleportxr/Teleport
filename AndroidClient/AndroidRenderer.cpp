#include "AndroidRenderer.h"
using namespace teleport;
using namespace android;

AndroidRenderer::AndroidRenderer(teleport::client::ClientDeviceState *clientDeviceState):clientrender::Renderer(new clientrender::NodeManager,new clientrender::NodeManager)
{
}

AndroidRenderer::~AndroidRenderer()
{
}

void AndroidRenderer::ConfigureVideo(const avs::VideoConfig &vc) 
{
}