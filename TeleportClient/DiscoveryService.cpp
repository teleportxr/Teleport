#include "DiscoveryService.h"
#include "Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/CommonNetworking.h"
#define RTC_ENABLE_WEBSOCKET 1
#include <rtc/websocket.hpp>
// This causes abort to be called, don't use it. Have to use exceptions.
//#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>
using nlohmann::json;

#include <fmt/core.h>

using namespace teleport;
using namespace client;
static teleport::client::DiscoveryService *discoveryService=nullptr;


teleport::client::DiscoveryService &teleport::client::DiscoveryService::GetInstance()
{
	if (!discoveryService)
	{
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
	cyclePorts={ 8080};//,80,443,10600,10700,10800};
}

DiscoveryService::~DiscoveryService()
{
}
/*
void DiscoveryService::SetClientID(uint64_t inClientID)
{
	clientID = inClientID;
}*/
void DiscoveryService::ReceiveWebSocketsMessage(uint64_t server_uid,std::string msg)
{
	std::lock_guard lock(signalingServersMutex);
	std::shared_ptr<SignalingServer> &signalingServer = signalingServers[server_uid];
	if(signalingServer)
		signalingServer->ReceiveMessage(msg);
}

void DiscoveryService::ReceiveBinaryWebSocketsMessage(uint64_t server_uid, std::vector<std::byte> bin)
{
	std::lock_guard lock(signalingServersMutex);
	std::shared_ptr<SignalingServer> &signalingServer = signalingServers[server_uid];
	if (signalingServer)
		signalingServer->ReceiveBinaryMessage(bin);
}

void DiscoveryService::ResetConnection(uint64_t server_uid,std::string url, uint16_t serverDiscoveryPort)
{
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
	signalingServer->Reset();
	std::shared_ptr<rtc::WebSocket> ws = signalingServer->webSocket;
	size_t first_slash=url.find('/');
	std::string base_url = url;
	std::string path;
	if (first_slash < url.length())
	{
		base_url=url.substr(0,first_slash);
		path=url.substr(first_slash+1,url.length()-first_slash-1);
	}
	std::string ws_url = fmt::format("ws://{0}:{1}/{2}", base_url, serverDiscoveryPort,path);
	
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
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
	if(!signalingServer)
		return;
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

void DiscoveryService::Disconnect(uint64_t server_uid)
{
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
	if(signalingServer)
		signalingServer->QueueDisconnectionMessage();
}

uint64_t DiscoveryService::Discover(uint64_t server_uid, std::string url, uint16_t signalPort)
{
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer= signalingServers[server_uid];
	}
	if (!signalingServer)
	{
		signalingServer = std::make_shared<SignalingServer>();
		signalingServers[server_uid]=signalingServer;
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
				json message = {{"teleport-signal-type", "request"}, {"content", {{"teleport", "0.9"}, {"clientID", signalingServer->clientID}}}};
				ws->send(message.dump());
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
	if (signalingServer->awaiting)
	{
		signalingServer->awaiting = false;
		serverDiscovered = true;
	}

	if(serverDiscovered)
	{
		return signalingServer->clientID;
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
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
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
			signalingServer->SendMessages();
		}
	}
	catch(std::exception& e)
	{
		TELEPORT_CERR << (e.what() ? e.what() : "Unknown exception") << std::endl;
	}
	catch(...)
	{
	}
	try
	{
		signalingServer->ProcessReceivedMessages();
	}
	catch(std::exception& e)
	{
		TELEPORT_CERR << (e.what() ? e.what() : "Unknown exception") << std::endl;
	}
	catch(...)
	{
		TELEPORT_CERR <<  "Unknown exception" << std::endl;
	}
}

bool DiscoveryService::GetNextMessage(uint64_t server_uid,std::string& msg)
{
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
	if (!signalingServer)
		return false;
	return signalingServer->GetNextPassedOnMessage(msg);
}

bool DiscoveryService::GetNextBinaryMessage(uint64_t server_uid, std::vector<uint8_t> &bin)
{
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
	if (!signalingServer)
		return false;
	return signalingServer->GetNextBinaryMessageReceived(bin);
}

void DiscoveryService::Send(uint64_t server_uid,std::string msg)
{
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
	if(signalingServer)
		signalingServer->QueueMessage(msg);
}

void DiscoveryService::SendBinary(uint64_t server_uid, std::vector<uint8_t> bin)
{
	std::shared_ptr<SignalingServer> signalingServer;
	{
		std::lock_guard lock(signalingServersMutex);
		signalingServer = signalingServers[server_uid];
	}
	if (signalingServer)
	{
		signalingServer->QueueBinaryMessage(bin);
	}
}