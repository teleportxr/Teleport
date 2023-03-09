#pragma once
#include <string>
#include <map>
#include <mutex>
#include "libavstream/common.hpp"
#include "Export.h"
#include "ClientData.h"
#include "DefaultHTTPService.h"
#include "DiscoveryService.h"

TELEPORT_EXPORT bool Client_StartSession(avs::uid clientID, std::string clientIP, int discovery_port);
TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void Client_StopSession(avs::uid clientID);
TELEPORT_EXPORT void Client_StartStreaming(avs::uid clientID);


typedef void(TELEPORT_STDCALL* ProcessAudioInputFn) (avs::uid uid, const uint8_t* data, size_t dataSize);
namespace teleport
{
	namespace server
	{
		extern std::mutex audioMutex;
		extern std::mutex videoMutex;
		extern std::map<avs::uid, ClientData> clientServices;
		extern ServerSettings serverSettings;

		extern std::shared_ptr<DiscoveryService> discoveryService;
		extern std::unique_ptr<DefaultHTTPService> httpService;
		extern SetHeadPoseFn setHeadPose;
		extern SetControllerPoseFn setControllerPose;
		extern ProcessNewInputFn processNewInput;
		extern DisconnectFn onDisconnect;
		extern ProcessAudioInputFn processAudioInput;
		extern GetUnixTimestampFn getUnixTimestamp;
		extern ReportHandshakeFn reportHandshake;
		extern uint32_t connectionTimeout ;

		extern ClientManager clientManager;
	}
}