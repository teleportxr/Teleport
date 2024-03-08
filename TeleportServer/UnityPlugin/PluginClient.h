#pragma once
#include <string>
#include <map>
#include <mutex>
#include "libavstream/common.hpp"
#include "Export.h"
#include "ClientData.h"
#include "SignalingService.h"

TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void Client_StopSession(avs::uid clientID);
