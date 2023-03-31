#pragma once

#include <memory>

#include "ClientNetworkContext.h"
#include "CustomAudioStreamTarget.h"
using namespace teleport;
using namespace server;

extern void Client_ProcessAudioInput(avs::uid clientID, const uint8_t* data, size_t dataSize);

ClientNetworkContext::ClientNetworkContext()
{
}

void ClientNetworkContext::Init(avs::uid clientID,bool receive_audio)
{
	audioStreamTarget.SetPlayCallback(std::bind(&Client_ProcessAudioInput, clientID, std::placeholders::_1, std::placeholders::_2));

	// Receiving
	if (receive_audio)
	{
		sourceAudioQueue.configure(8192, 120, "SourceAudioQueue");
		audioDecoder.configure(100);
		audioTarget.configure(&audioStreamTarget);
	}
}