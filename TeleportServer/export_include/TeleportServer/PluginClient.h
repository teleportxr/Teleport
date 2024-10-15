#pragma once
#include <stdint.h>
#include "libavstream/common_maths.h"
#include "TeleportServer/Export.h"

namespace avs
{
	typedef uint64_t uid;
}
namespace teleport
{
	namespace server
	{	
		struct ClientSettings;
		struct VideoEncodeParams;
	}
}
TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void Client_StopSession(avs::uid clientID);
TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID,avs::uid originNode);

TELEPORT_EXPORT bool Client_StreamNode(avs::uid clientID, avs::uid nodeID);
TELEPORT_EXPORT bool Client_UnstreamNode(avs::uid clientID, avs::uid nodeID);

TELEPORT_EXPORT void Client_SetClientSettings(avs::uid clientID,const teleport::server::ClientSettings &clientSettings);
TELEPORT_EXPORT bool Client_SetVideoEncodeParams(avs::uid clientID,const teleport::server::VideoEncodeParams &params);
TELEPORT_EXPORT bool Client_VideoEncodePipelineProcess(avs::uid clientID,bool forceIDR);
TELEPORT_EXPORT avs::AxesStandard Client_GetAxesStandard(avs::uid clientID);