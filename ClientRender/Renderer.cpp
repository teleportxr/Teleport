#include "ClientRender/Renderer.h"
#include <libavstream/libavstream.hpp>
#include "TeleportClient/ServerTimestamp.h"

avs::Timestamp clientrender::platformStartTimestamp ;
static bool timestamp_initialized=false;
using namespace clientrender;

Renderer::Renderer()
	:localGeometryCache(new clientrender::NodeManager())
	, geometryCache(new clientrender::NodeManager())
{
	if (!timestamp_initialized)
#ifdef _MSC_VER
		platformStartTimestamp = avs::PlatformWindows::getTimestamp();
#else
		platformStartTimestamp = avs::PlatformPOSIX::getTimestamp();
#endif
	timestamp_initialized=true;
}

void Renderer::Update(double timestamp_ms)
{
	double timeElapsed_s = (timestamp_ms - previousTimestamp) / 1000.0f;//ms to seconds

	teleport::client::ServerTimestamp::tick(timeElapsed_s);

	geometryCache.Update(static_cast<float>(timeElapsed_s));
	resourceCreator.Update(static_cast<float>(timeElapsed_s));

	localGeometryCache.Update(static_cast<float>(timeElapsed_s));
	localResourceCreator.Update(static_cast<float>(timeElapsed_s));

	previousTimestamp = timestamp_ms;
}