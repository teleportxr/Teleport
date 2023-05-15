#include "DiscoveryService.h"
#include "Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/CommonNetworking.h"
#define RTC_ENABLE_WEBSOCKET 1
#include <rtc/websocket.hpp>
#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>
using nlohmann::json;

#include <fmt/core.h>

using namespace teleport;
using namespace client;
static teleport::client::DiscoveryService *discoveryService=nullptr;

uint16_t teleport::client::SignalingServer::GetPort() const
{
	uint16_t p = remotePort;
	if (p == 0)
		p = teleport::client::DiscoveryService::GetInstance().cyclePorts[cyclePortIndex];
	return p;
}

teleport::client::DiscoveryService &teleport::client::DiscoveryService::GetInstance()
{
	if (!discoveryService)
	{
		if (enet_initialize() != 0)
		{
			TELEPORT_CERR << "An error occurred while attempting to initalise ENet!\n";
		}
		discoveryService = new teleport::client::DiscoveryService;
	}
	return *discoveryService;
}

void teleport::client::DiscoveryService::ShutdownInstance()
{
	delete discoveryService;
	discoveryService=nullptr;
}

#ifdef _MSC_VER
#define MAKE_ENET_BUFFER(x) {sizeof(x) ,(void*)&x};
#else
#define MAKE_ENET_BUFFER(x) {(void*)&x,sizeof(x)};
#endif

DiscoveryService::DiscoveryService()
{
	cyclePorts={ 8080,80,443,10600,10700,10800};
}

DiscoveryService::~DiscoveryService()
{
}

void DiscoveryService::SetClientID(uint64_t inClientID)
{
	clientID = inClientID;
}

void DiscoveryService::ReceiveWebSocketsMessage(uint64_t server_uid,std::string msg)
{
	TELEPORT_COUT << ": info: ReceiveWebSocketsMessage " << msg << std::endl;
	std::lock_guard lock(messagesReceivedMutex);
	messagesReceived.push(msg);
}

void DiscoveryService::ReceiveBinaryWebSocketsMessage(uint64_t server_uid,std::vector<std::byte> bin)
{
	TELEPORT_COUT << ": info: ReceiveBinaryWebSocketsMessage." << std::endl;
	std::lock_guard lock(binaryMessagesReceivedMutex);
	binaryMessagesReceived.push(bin);
}

void DiscoveryService::ResetConnection(uint64_t server_uid,std::string url, uint16_t serverDiscoveryPort)
{
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
		std::lock_guard lock(messagesToSendMutex);
		while (!messagesToSend.empty())
			messagesToSend.pop();
	}
	{
		std::lock_guard lock(binaryMessagesReceivedMutex);
		while(!binaryMessagesReceived.empty())
			binaryMessagesReceived.pop();
	}
	{
		std::lock_guard lock(binaryMessagesToSendMutex);
		while (!binaryMessagesToSend.empty())
			binaryMessagesToSend.pop();
	}
	std::shared_ptr<SignalingServer> &signalingServer = signalingServers[server_uid];
	std::shared_ptr<rtc::WebSocket> ws = signalingServer->webSocket;
	std::string ws_url=fmt::format("ws://{0}:{1}",url, serverDiscoveryPort);
	TELEPORT_COUT << "Websocket open() " << ws_url << std::endl;
	if(url.length()>0)
	{
		try
		{
			ws->open(ws_url);
		}
		catch(std::exception& e)
		{
			TELEPORT_CERR << (e.what() ? e.what() : "Unknown exception") << std::endl;
		}
		catch(...)
		{
		}
	}
}

void DiscoveryService::InitSocket(uint64_t server_uid)
{
	rtc::WebSocket::Configuration config;
	std::shared_ptr<SignalingServer> &signalingServer=signalingServers[server_uid];
	auto ws = signalingServer->webSocket = std::make_shared<rtc::WebSocket>(config);
	if (signalingServer->remotePort == 0)
	{
		signalingServer->cyclePortIndex++;
		signalingServer->cyclePortIndex %= cyclePorts.size();
		TELEPORT_COUT << "Cycling ports: connecting to " << signalingServer->url << " on port " << cyclePorts[signalingServer->cyclePortIndex] << std::endl;
	}
	auto receiveWebSocketMessage = [this,server_uid](const rtc::message_variant message)
	{
		if(std::holds_alternative<std::string>(message))
		{
			std::string msg = std::get<std::string>(message);
			ReceiveWebSocketsMessage(server_uid,msg);
		}
		else if(std::holds_alternative<rtc::binary>(message))
		{
			rtc::binary bin=std::get<rtc::binary>(message);
			ReceiveBinaryWebSocketsMessage(server_uid,bin);
		}
	};
	ws->onError([this, server_uid](std::string error)
	{
		auto& s = signalingServers[server_uid];
		if (s)
		{
			TELEPORT_CERR << "Websocket error " << error << " for url " <<s->url<<" on port "<<s->GetPort()<< std::endl;
			s->webSocket.reset();
		}
	});
	ws->onMessage(receiveWebSocketMessage);
	ws->onOpen([this,server_uid]()
	{
		TELEPORT_CERR << "Websocket onOpen " << std::endl;
	});
	uint16_t remotePort = signalingServer->GetPort();
	ResetConnection(server_uid, signalingServer->url, remotePort);
	frame = 2;
}

uint64_t DiscoveryService::Discover(uint64_t server_uid, std::string url, uint16_t signalPort)
{
	std::lock_guard lock(signalingServersMutex);
	std::shared_ptr<SignalingServer> &signalingServer= signalingServers[server_uid];
	if (!signalingServer)
	{
		signalingServer = std::make_shared<SignalingServer>();
	}
	if(signalingServer->url!=url||signalingServer->remotePort!=signalPort)
	{
		signalingServer->webSocket.reset();
		signalingServer->remotePort = signalPort;
		signalingServer->url = url;
		signalingServer->uid = server_uid;
	}
	if(!signalingServer->url.length())
		return 0;
	bool serverDiscovered = false;
	std::shared_ptr<rtc::WebSocket> ws = signalingServer->webSocket;
	if(!ws)
	{
		InitSocket(server_uid);
	}
	if (!ws)
	{
		return 0;
	}
	if (url.empty())
	{
		url = "255.255.255.255";
	}
	// Send our client id to the server on the discovery port. Once every 1000 frames.
	frame--;
	if(!frame)
	{
		try
		{
			if (ws->isOpen())
			{
				json message = { {"teleport-signal-type","request"},{"content",{ {"teleport","0.9"},{"clientID", clientID}}} };
				//rtc::message_variant wsdata = fmt::format("{{\"clientID\":{}}}",clientID);
				ws->send(message.dump());
				//TELEPORT_CERR << "Websocket send()" << std::endl;
				//TELEPORT_CERR << "webSocket->send: " << message.dump() << "  .\n";
				frame = 100;
			}
			else if (ws->readyState() == rtc::WebSocket::State::Closed)
			{
				ResetConnection(server_uid, signalingServer->url, signalingServer->remotePort);
				frame = 2;
			}
			else
			{
				frame = 1;
			}
		}
		catch (std::exception& e)
		{
			TELEPORT_CERR<<(e.what()?e.what():"Unknown exception")<< std::endl;
		}
		catch(...)
		{
		}
	}
	if(awaiting)
	{
		awaiting = false;
		serverDiscovered = true;
	}

	if(serverDiscovered)
	{
		return clientID;
	}
	return 0;
}

void DiscoveryService::Tick()
{
	for (auto i : signalingServers)
	{
		Tick(i.first);
	}
}

void DiscoveryService::Tick(uint64_t server_uid)
{
	std::lock_guard lock(signalingServersMutex);
	std::shared_ptr<SignalingServer> &signalingServer = signalingServers[server_uid];
	if (!signalingServer)
		return;
	std::shared_ptr<rtc::WebSocket>& ws = signalingServer->webSocket;
	if(!ws)
	{
		InitSocket(server_uid);
	}
	if (!ws)
	{
		return;
	}
	try
	{
		if (ws->isOpen())
		{
			{
				std::lock_guard lock(messagesToSendMutex);
				while (messagesToSend.size())
				{
					ws->send(messagesToSend.front());
					//TELEPORT_CERR << "webSocket->send: " << messagesToSend.front() << "  .\n";
					messagesToSend.pop();
				}
			}
			{

				std::lock_guard lock(binaryMessagesToSendMutex);
				while (binaryMessagesToSend.size())
				{
					auto& bin = binaryMessagesToSend.front();
					ws->send(bin.data(), bin.size());
					//TELEPORT_CERR << "webSocket->send binary: " << bin.size() << " bytes.\n";
					binaryMessagesToSend.pop();
				}
			}
		}
	}
	catch(std::exception& e)
	{
		TELEPORT_CERR << (e.what() ? e.what() : "Unknown exception") << std::endl;
	}
	catch(...)
	{
	}
	{
		std::lock_guard lock(messagesReceivedMutex);
		while(messagesReceived.size())
		{
			std::string msg = messagesReceived.front();
			messagesReceived.pop();
			if(!msg.length())
				continue;
			json message=json::parse(msg);
			if(message.contains("teleport-signal-type"))
			{
				std::string type = message["teleport-signal-type"];
				if(type=="request-response")
				{
					if(message.contains("content"))
					{
						json content = message["content"];
						if(content.contains("clientID"))
						{
							uint64_t clid = content["clientID"];
							if(clientID!=clid)
								clientID=clid;
							if(clientID!=0)
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
}

bool DiscoveryService::GetNextMessage(uint64_t server_uid,std::string& msg)
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

bool DiscoveryService::GetNextBinaryMessage(uint64_t server_uid,std::vector<uint8_t>& msg)
{
	std::lock_guard lock(binaryMessagesReceivedMutex);
	if (binaryMessagesReceived.size())
	{
		auto &bin=binaryMessagesReceived.front();
		msg.resize(bin.size());
		memcpy(msg.data(),bin.data(),msg.size());
		binaryMessagesReceived.pop();
		return true;
	}
	return false;
}
void DiscoveryService::Send(uint64_t server_uid,std::string msg)
{
	std::lock_guard lock(messagesToSendMutex);
	messagesToSend.push(msg);
}

void DiscoveryService::SendBinary(uint64_t server_uid, std::vector<uint8_t> bin)
{
	std::lock_guard lock(binaryMessagesToSendMutex);
	std::vector<std::byte> b;
	b.resize(bin.size());
	memcpy(b.data(),bin.data(),b.size());
	binaryMessagesToSend.push(b);

}