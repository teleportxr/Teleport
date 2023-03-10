#include "ClientPipeline.h"
#include <libavstream/network/webrtc_networksource.h>
#include <libavstream/network/srt_efp_networksource.h>

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
