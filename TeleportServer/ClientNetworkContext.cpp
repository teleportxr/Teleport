#pragma once

#include <memory>

#include "ClientNetworkContext.h"
#include "TeleportAudio/CustomAudioStreamTarget.h"
#include "ClientManager.h"
#include "Exports.h"
using namespace teleport;
using namespace server;

void ProcessAudioInput(avs::uid clientID, const uint8_t *data, size_t dataSize)
{
	processAudioInput(clientID, data, dataSize);
}



ClientNetworkContext::ClientNetworkContext()
{
}

void ClientNetworkContext::Init(avs::uid clientID,bool receive_audio)
{
	audioStreamTarget.SetPlayCallback(std::bind(&ProcessAudioInput, clientID, std::placeholders::_1, std::placeholders::_2));

	// Receiving
	if (receive_audio)
	{
		sourceAudioQueue.configure(8192, 120, "SourceAudioQueue");
		audioDecoder.configure(100);
		audioTarget.configure(&audioStreamTarget);
	}
}