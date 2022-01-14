#include "ClientRender/Renderer.h"
#include <libavstream/libavstream.hpp>

avs::Timestamp clientrender::platformStartTimestamp ;
static bool timestamp_initialized=false;
using namespace clientrender;

Renderer::Renderer()
{
	if (!timestamp_initialized)
#ifdef _MSC_VER
		platformStartTimestamp = avs::PlatformWindows::getTimestamp();
#else
		platformStartTimestamp = avs::PlatformPOSIX::getTimestamp();
#endif
	timestamp_initialized=true;
}