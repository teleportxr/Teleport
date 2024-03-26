#include "SignalingServer.h"
#include "Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/CommonNetworking.h"
#include "DiscoveryService.h"
#define RTC_ENABLE_WEBSOCKET 1
#include <rtc/websocket.hpp>
// This causes abort to be called, don't use it. Have to use exceptions.
//#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>
using nlohmann::json;

#include <fmt/core.h>

using namespace teleport;
using namespace client;


uint16_t teleport::client::SignalingServer::GetPort() const
{
	uint16_t p = remotePort;
	if (p == 0)
		p = teleport::client::DiscoveryService::GetInstance().cyclePorts[cyclePortIndex];
	return p;
}
void teleport::client::SignalingServer::Reset()
{
	{
		std::lock_guard lock(messagesToSendMutex);
		while (!messagesToSend.empty())
			messagesToSend.pop();
	}
	{
		std::lock_guard lock(messagesReceivedMutex);
		while (!messagesReceived.empty())
			messagesReceived.pop();
	}
	{
		std::lock_guard lock(messagesToPassOnMutex);
		while (!messagesToPassOn.empty())
			messagesToPassOn.pop();
	}
	{
		std::lock_guard lock(binaryMessagesReceivedMutex);
		while (!binaryMessagesReceived.empty())
			binaryMessagesReceived.pop();
	}
	{
		std::lock_guard lock(binaryMessagesToSendMutex);
		while (!binaryMessagesToSend.empty())
			binaryMessagesToSend.pop();
	}
}
void SignalingServer::SendMessages()
{
	{
		std::lock_guard lock(messagesToSendMutex);
		while (messagesToSend.size())
		{
			if (webSocket)
				webSocket->send(messagesToSend.front());
			// TELEPORT_CERR << "webSocket->send: " << messagesToSend.front() << "  .\n";
			messagesToSend.pop();
		}
	}
	{
		std::lock_guard lock(binaryMessagesToSendMutex);
		while (binaryMessagesToSend.size())
		{
			auto &bin = binaryMessagesToSend.front();
			webSocket->send(bin.data(), bin.size());
			// TELEPORT_CERR << "webSocket->send binary: " << bin.size() << " bytes.\n";
			binaryMessagesToSend.pop();
		}
	}
}
void SignalingServer::QueueMessage(const std::string &msg)
{
	std::lock_guard lock(messagesToSendMutex);
	messagesToSend.push(msg);
}
void SignalingServer::ReceiveMessage(const std::string &msg)
{
	TELEPORT_COUT << ": info: ReceiveWebSocketsMessage " << msg << std::endl;
	std::lock_guard lock(messagesReceivedMutex);
	messagesReceived.push(msg);
}
void SignalingServer::ReceiveBinaryMessage(const std::vector<std::byte> &bin)
{
	TELEPORT_COUT << ": info: ReceiveBinaryWebSocketsMessage." << std::endl;
	std::lock_guard lock(binaryMessagesReceivedMutex);
	binaryMessagesReceived.push(bin);
}

void SignalingServer::ProcessReceivedMessages()
{
	std::lock_guard lock(messagesReceivedMutex);
	while (messagesReceived.size())
	{
		std::string msg = messagesReceived.front();
		messagesReceived.pop();
		if (!msg.length())
			continue;
		json message = json::parse(msg);
		if (message.contains("teleport-signal-type"))
		{
			std::string type = message["teleport-signal-type"];
			if (type == "request-response")
			{
				if (message.contains("content"))
				{
					json content = message["content"];
					if (content.contains("clientID"))
					{
						uint64_t clid = content["clientID"];
						if (clientID != clid)
							clientID = clid;
						if (clientID != 0)
						{
							awaiting = true;
						}
					}
				}
			}
			else
			{
				std::lock_guard lock(messagesToPassOnMutex);
				messagesToPassOn.push(msg);
			}
		}
		else
		{
			std::lock_guard lock(messagesToPassOnMutex);
			messagesToPassOn.push(msg);
		}
	}
}

bool SignalingServer::GetNextPassedOnMessage(std::string &msg)
{
	std::lock_guard lock(messagesToPassOnMutex);
	if (messagesToPassOn.size())
	{
		msg = messagesToPassOn.front();
		messagesToPassOn.pop();
		return true;
	}
	return false;
}

bool SignalingServer::GetNextBinaryMessageReceived(std::vector<uint8_t> &bin)
{
	std::lock_guard lock(binaryMessagesReceivedMutex);
	if (binaryMessagesReceived.size())
	{
		auto &b = binaryMessagesReceived.front();
		bin.resize(b.size());
		memcpy(bin.data(), b.data(), b.size());
		binaryMessagesReceived.pop();
		return true;
	}
	return false;
}

void SignalingServer::QueueDisconnectionMessage()
{
	json message = {{"teleport-signal-type", "disconnect"}};
	QueueMessage(message.dump());
	awaiting = false;
}

void SignalingServer::QueueBinaryMessage(std::vector<uint8_t> &bin)
{
	std::lock_guard lock(binaryMessagesToSendMutex);
	std::vector<std::byte> b;
	b.resize(bin.size());
	memcpy(b.data(), bin.data(), b.size());
	binaryMessagesToSend.push(b);
}