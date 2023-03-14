#include "ClientPipeline.h"
#include <libavstream/network/webrtc_networksource.h>
#if TELEPORT_SUPPORT_SRT
#include <libavstream/network/srt_efp_networksource.h>
#endif
using namespace teleport;
using namespace client;

ClientPipeline::ClientPipeline()
{
	source.reset(new avs::WebRtcNetworkSource());
}

ClientPipeline::~ClientPipeline()
{
}


void ClientPipeline::Shutdown()
{
	pipeline.deconfigure();
}
