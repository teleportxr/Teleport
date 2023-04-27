#include "DiscoveryService.h"
#include "Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/CommonNetworking.h"
#define RTC_ENABLE_WEBSOCKET 1
#include <rtc/websocket.hpp>
#define JSON_NOEXCEPTION 1
#include <nlohmann/json.hpp>
using nlohmann::json;

using namespace teleport;
using namespace client;
static teleport::client::DiscoveryService *discoveryService=nullptr;

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
	serverAddress = {};
}

DiscoveryService::~DiscoveryService()
{
}

void DiscoveryService::SetClientID(uint64_t inClientID)
{
	clientID = inClientID;
}

ENetSocket DiscoveryService::CreateDiscoverySocket(std::string ip, uint16_t discoveryPort)
{
	ENetSocket socket = enet_socket_create(ENetSocketType::ENET_SOCKET_TYPE_DATAGRAM);// PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket <= 0)
	{
		TELEPORT_CLIENT_FAIL("Failed to create service discovery UDP socket");
		return 0;
	}

	int flagEnable = 1;
	enet_socket_set_option(socket, ENET_SOCKOPT_REUSEADDR, 1);
	enet_socket_set_option(socket, ENET_SOCKOPT_BROADCAST, 1);
	//enet_socket_set_option(sock, ENET_SOCKOPT_RCVBUF, 0);
	//enet_socket_set_option(sock, ENET_SOCKOPT_SNDBUF, 0);
	// We don't want to block, just check for packets.
	enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1);

	// Here we BIND the socket to the local address that we want to be identified with.
	// e.g. our OWN local IP.
	ENetAddress bindAddress = { ENET_HOST_ANY, discoveryPort };

	if (!ip.empty())
	{
	//	ip = "127.0.0.1";
		enet_address_set_host(&(bindAddress), ip.c_str());
	}
	if (enet_socket_bind(socket, &bindAddress) != 0)
	{
		TELEPORT_INTERNAL_CERR("Failed to bind to service discovery UDP socket");
#ifdef _MSC_VER
		int err = WSAGetLastError();
		TELEPORT_CERR << "enet_socket_bind failed with error " << err << std::endl;
#else
		TELEPORT_CERR << "enet_socket_bind failed with error " <<std::endl;
#endif
		enet_socket_destroy(socket);
		socket = 0;
		return 0;
	}
	return socket;
}

void DiscoveryService::ReceiveWebSocketsMessage(uint64_t server_uid,std::string msg)
{
	TELEPORT_CERR << "ReceiveWebSocketsMessage " << msg << std::endl;
	std::lock_guard lock(mutex);
	messagesReceived.push(msg);
}

void DiscoveryService::ResetConnection(uint64_t server_uid,std::string ip, uint16_t serverDiscoveryPort)
{
	while(!messagesReceived.empty())
		messagesReceived.pop();
	while(!messagesToPassOn.empty())
		messagesToPassOn.pop();
	while(!messagesToSend.empty())
		messagesToSend.pop();
	std::shared_ptr<rtc::WebSocket> ws=websockets[server_uid];
	TELEPORT_CERR << "Websocket open()" << std::endl;
	if(ip.length()>0)
	{
		if(serverDiscoveryPort)
			ws->open(ip+fmt::format(":{0}",serverDiscoveryPort));
		else
			ws->open(ip);
	}
}
void DiscoveryService::InitSocket(uint64_t server_uid)
{
	rtc::WebSocket::Configuration config;
	std::shared_ptr<rtc::WebSocket> ws=websockets[server_uid] = std::make_shared<rtc::WebSocket>(config);
	auto receiveWebSocketMessage = [this,server_uid](const rtc::message_variant message)
	{
		if(std::holds_alternative<std::string>(message))
		{
			std::string msg = std::get<std::string>(message);
			ReceiveWebSocketsMessage(server_uid,msg);
		}
	};
	ws->onError([this,server_uid](std::string error)
	{
		TELEPORT_CERR << "Websocket error " << error << std::endl;
		websockets.erase(server_uid);
	});
	ws->onMessage(receiveWebSocketMessage);
	ws->onOpen([this,server_uid]()
	{
		TELEPORT_CERR << "Websocket onOpen " << std::endl;
	});
	ResetConnection(server_uid,serverIP,serverDiscoveryPort);
	frame = 2;
}

uint64_t DiscoveryService::Discover(uint64_t server_uid, std::string ip, uint16_t signalPort, ENetAddress& remote)
{
	std::lock_guard lock(mutex);
	serverDiscoveryPort=signalPort;
	if (serverDiscoveryPort == 0)
	{
		serverDiscoveryPort = 8080;
	}
	serverIP = ip;
	if(!serverIP.length())
		return 0;
	bool serverDiscovered = false;
	std::shared_ptr<rtc::WebSocket> ws=websockets[server_uid];
	if(!ws)
	{
		InitSocket(server_uid);
		ws=websockets[server_uid];
	}
	if (ip.empty())
	{
		ip = "255.255.255.255";
	}
	// Send our client id to the server on the discovery port. Once every 1000 frames.
	frame--;
	if(!frame)
	{
		if(ws->isOpen())
		{
			json message = { {"teleport-signal-type","request"},{"content",{ {"teleport","0.9"},{"clientID", clientID}}}};
			//rtc::message_variant wsdata = fmt::format("{{\"clientID\":{}}}",clientID);
			ws->send(message.dump());
			TELEPORT_CERR << "Websocket send()" << std::endl;
			TELEPORT_CERR << "webSocket->send: " << message.dump() << "  .\n";
			frame = 1000;
		}
		else if(ws->readyState()==rtc::WebSocket::State::Closed)
		{
			ResetConnection(server_uid,ip,serverDiscoveryPort);
			frame = 2;
		}
		else
		{
			frame = 1;
		}
	}
	if(awaiting)
	{
		auto f = fobj.wait_for(std::chrono::microseconds(0));
		if (f == std::future_status::timeout)
		{
			return 0;
		}
		if (f == std::future_status::ready)
		{
			awaiting = false;
			int result= fobj.get();
			if(result!=0)
			{
			#ifdef _MSC_VER
				int err = WSAGetLastError();
				TELEPORT_CERR << "enet_address_set_host failed with error " << err << std::endl;
			#else
				TELEPORT_CERR << "enet_address_set_host failed with error " << result<< std::endl;
			#endif
				return 0;
			}
			serverIP = ip;
			remote = serverAddress;
			remote.port = remotePort;
			serverDiscovered = true;
		}
		else
		{
			return 0;
		}
	}

	if(serverDiscovered)
	{
		char remoteIP[20];
		enet_address_get_host_ip(&remote, remoteIP, sizeof(remoteIP));
		TELEPORT_CLIENT_LOG("Discovered session server: %s:%d\n", remoteIP, remote.port);
		return clientID;
	}
	return 0;
}

void DiscoveryService::Tick(uint64_t server_uid)
{
	if(!serverIP.length())
		return;
	std::shared_ptr<rtc::WebSocket> ws=websockets[server_uid];
	if(!ws)
	{
		InitSocket(server_uid);
		ws=websockets[server_uid];
	}
	if(ws->isOpen())
	{
		while(messagesToSend.size())
		{
			ws->send(messagesToSend.front());
			TELEPORT_CERR << "webSocket->send: " << messagesToSend.front() << "  .\n";
			messagesToSend.pop();
		}
	}
	static size_t bytesRecv;
	while(messagesReceived.size())
	{
		std::string msg = messagesReceived.front();
		messagesReceived.pop();
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
							if(content.contains("servicePort"))
							{
								remotePort = content["servicePort"];
								if (!awaiting)
								{
									serverAddress.host = ENET_HOST_ANY;
									serverAddress.port = remotePort;
									fobj = std::async(&enet_address_set_host, &(serverAddress), serverIP.c_str());
									awaiting = true;
								}
								
								break;
							}
						}
					}
				}
				messagesReceived.pop();
			}
			else
				messagesToPassOn.push(msg);
		}
		else
			messagesToPassOn.push(msg);
	}
}

bool DiscoveryService::GetNextMessage(uint64_t server_uid,std::string& msg)
{
	std::lock_guard lock(mutex);
	if (messagesToPassOn.size())
	{
		msg = messagesToPassOn.front();
		messagesToPassOn.pop();
		return true;
	}
	return false;
}
void DiscoveryService::Send(uint64_t server_uid,std::string msg)
{
	messagesToSend.push(msg);
}
