#pragma once
#include <string>
#include <map>
#include <mutex>
#include "libavstream/common.hpp"
#include "Export.h"
#include "ClientData.h"
#include "DefaultHTTPService.h"
#include "SignalingService.h"

TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void Client_StopSession(avs::uid clientID);


typedef void(TELEPORT_STDCALL* ProcessAudioInputFn) (avs::uid uid, const uint8_t* data, size_t dataSize);
namespace teleport
{
	namespace server
	{
		extern std::mutex audioMutex;
		extern std::mutex videoMutex;
		
		extern ServerSettings serverSettings;

		extern std::unique_ptr<DefaultHTTPService> httpService;
		extern SetHeadPoseFn setHeadPose;
		extern SetControllerPoseFn setControllerPose;
		extern ProcessNewInputStateFn processNewInputState;
		extern ProcessNewInputEventsFn processNewInputEvents;
		extern DisconnectFn onDisconnect;
		extern ProcessAudioInputFn processAudioInput;
		extern GetUnixTimestampFn getUnixTimestamp;
		extern ReportHandshakeFn reportHandshake;
		extern uint32_t connectionTimeout ;

		extern ClientManager clientManager;
	}
}