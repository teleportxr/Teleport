#pragma once
#include <stdint.h>
#include "TeleportServer/Export.h"

namespace avs
{
	typedef uint64_t uid;
}

TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void Client_StopSession(avs::uid clientID);
TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID,avs::uid originNode);

TELEPORT_EXPORT bool Client_StreamNode(avs::uid clientID, avs::uid nodeID);
TELEPORT_EXPORT bool Client_UnstreamNode(avs::uid clientID, avs::uid nodeID);