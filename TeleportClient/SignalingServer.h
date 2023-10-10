#pragma once

#include <cstdint>
#include <string>
// for std::future,await:
#include <future>
#include <queue>
#include <unordered_map>

namespace rtc
{
	class WebSocket;
}
namespace teleport
{
	namespace client
	{
		//! Identifies a remote server as seen by the DiscoveryService.
		struct SignalingServer
		{
			std::string url;
			uint16_t remotePort = 0;
			uint64_t uid = 0;
			uint8_t cyclePortIndex = 0;
			std::shared_ptr<rtc::WebSocket> webSocket;
			uint16_t GetPort() const;
			void Reset();
			void QueueMessage(const std::string &msg);
			void QueueBinaryMessage(std::vector<uint8_t> &bin);
			void QueueDisconnectionMessage();
			void ReceiveMessage(const std::string &msg);
			void ReceiveBinaryMessage(const std::vector<std::byte> &bin);
			bool GetNextPassedOnMessage(std::string &msg);
			bool GetNextBinaryMessageReceived(std::vector<uint8_t> &bin);
			void SendMessages();
			void ProcessReceivedMessages();
			std::queue<std::string> messagesToSend;
			std::mutex messagesToSendMutex;
			std::mutex messagesReceivedMutex;
			std::mutex binaryMessagesReceivedMutex;
			std::mutex messagesToPassOnMutex;
			std::mutex binaryMessagesToSendMutex;
			std::queue<std::string> messagesReceived;
			std::queue<std::string> messagesToPassOn;
			std::queue<std::vector<std::byte>> binaryMessagesReceived;
			std::queue<std::vector<std::byte>> binaryMessagesToSend;
			uint64_t clientID = uint64_t(0x0);
			bool awaiting = false;
		};
	}
}