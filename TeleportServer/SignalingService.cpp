#include "SignalingService.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportServer/ClientData.h"
#include "TeleportServer/ServerSettings.h"    
#include "TeleportUtility.h" 
#include "UnityPlugin/PluginMain.h"
#include "UnityPlugin/PluginClient.h"
#include <rtc/websocket.hpp>
#include <rtc/websocketserver.hpp>
#include <functional>
#include <regex>

using nlohmann::json;

using namespace teleport;
using namespace server;
extern ServerSettings casterSettings;
TELEPORT_EXPORT bool Client_StartSession(avs::uid clientID, std::string clientIP, int discovery_port);
TELEPORT_EXPORT void AddUnlinkedClientID(avs::uid clientID);

bool SignalingService::initialize(uint16_t discovPort, uint16_t servPort, std::string desIP)
{
	if (discovPort != 0)
		discoveryPort = discovPort;

	if (discoveryPort == 0)
	{
		TELEPORT_CERR <<"Discovery port is not set.\n";
		return false;
	}

	if (!webSocketServer)
	{
		rtc::WebSocketServer::Configuration config;
		config.port = discovPort;
		webSocketServer = std::make_shared<rtc::WebSocketServer>(config);
		auto onWebSocketClient =std::bind(&SignalingService::OnWebSocket,this,std::placeholders::_1);
		webSocketServer->onClient(onWebSocketClient);
	}
	if (servPort!= 0)
		servicePort = servPort;

	if (servicePort == 0)
	{
		TELEPORT_CERR <<"Service port is not set.\n";
		return false;
	}
	if(discoverySocket!=0)
	{
		TELEPORT_CERR << "Discovery socket is already set.\n";
		return false;
	}
	discoverySocket = enet_socket_create(ENetSocketType::ENET_SOCKET_TYPE_DATAGRAM);
	if (discoverySocket <= 0)
	{
		TELEPORT_CERR <<"Failed to create discovery socket.\n";
		return false;
	}

	desiredIP = desIP;
	if (enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_NONBLOCK, 1)<0)
	{
		TELEPORT_CERR <<"Failed to set nonblock.\n";
		return false;
	}
	if (enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_BROADCAST, 1)<0)
	{
		TELEPORT_CERR <<"Failed to set broadcast.\n";
		return false;
	}
	if (enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_REUSEADDR, 1)<0)
	{
		TELEPORT_CERR <<"Failed to set re-use address.\n";
		return false;
	}

	address = { ENET_HOST_ANY, discoveryPort };
	if (enet_socket_bind(discoverySocket, &address) != 0)
	{
		#ifdef _MSC_VER
		int err= WSAGetLastError();
		TELEPORT_CERR << "Failed to bind discovery socket on port: " << address.port << " with error "<<err<<"\n";
		#endif
		enet_socket_destroy(discoverySocket);
		discoverySocket = 0;
		return false;
	}

	return true;
}

void SignalingService::ReceiveWebSocketsMessage(avs::uid clientID, std::string msg)
{
	std::lock_guard<std::mutex> lock(webSocketsMessagesMutex);
	auto &c=signalingClients[clientID];
	if(c)
		c->messagesReceived.push_back(msg);
	else
	{
		TELEPORT_CERR << "Websocket message received but already removed the SignalingClient " << clientID << std::endl;
	}
}

bool SignalingService::GetNextMessage(avs::uid clientID, std::string& msg)
{
	std::lock_guard<std::mutex> lock(webSocketsMessagesMutex);
	auto& c = signalingClients[clientID];
	if (c&&!c->messagesToPassOn.empty())
	{
		msg = c->messagesToPassOn.front();
		c->messagesToPassOn.pop();
		return true;
	}
	return false;
}
void SignalingService::OnWebSocket(std::shared_ptr<rtc::WebSocket> ws)
{
	avs::uid clientID = TeleportUtility::GenerateID();
	std::string addr = ws->remoteAddress().value();

	std::regex re("([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)(:[0-9]+)?", std::regex_constants::icase | std::regex::extended);
	std::smatch match;
	std::string ip_addr_port;
	if (std::regex_search(addr, match, re))
	{
		ip_addr_port = match.str(1);
	}
	else
	{
		TELEPORT_CERR << "Websocket connection from " << addr <<" - couldn't decode address." <<std::endl;
		return;
	}
	signalingClients[clientID] = std::make_shared<SignalingClient>();
	clientUids.insert(clientID);
	auto& c = signalingClients[clientID];
	c->webSocket = ws;
	c->address = ip_addr_port;
	auto recv = [this, clientID](rtc::message_variant message) {
		if (std::holds_alternative<std::string>(message))
		{
			std::string msg = std::get<std::string>(message);
			ReceiveWebSocketsMessage(clientID, msg);
		}
	};
	ws->onMessage(recv);
	ws->onError([this, clientID](std::string error)
		{
			TELEPORT_CERR << "Websocket err " << error << std::endl;
	;});
}
void SignalingService::shutdown()
{
	webSocketServer.reset();
	enet_socket_destroy(discoverySocket);
	discoverySocket = 0;
	newClients.clear();
}


void SignalingService::processInitialRequest(avs::uid uid, std::shared_ptr<SignalingClient>& discoveryClient,json& content)
{
	if (content.find("clientID") != content.end())
	{
		avs::uid clientID = 0;
		auto& j_clientID = content["clientID"];
		clientID = j_clientID.get<unsigned long long>();
		if (clientID == 0)
			clientID = uid;
		else
		{
			if (signalingClients.find(clientID) == signalingClients.end())
			{
				// sent us a client ID that isn't valid. Ignore it, don't waste bandwidth..?
				// or instead, send the replacement ID in the response, leave it up to
				// client whether they accept the new ID or abandon the connection.

			}
			// identifies as a previous client. Discard the new client ID.
			//TODO: we're taking the client's word for it that it is clientID. Some kind of token/hash?
			signalingClients[clientID] = discoveryClient;
			clientUids.insert(clientID);
			if (uid != clientID)
			{
				signalingClients[uid] = nullptr;
				uid = clientID;
			}
		}
		std::string ipAddr;
		ipAddr = discoveryClient->address;
		TELEPORT_COUT << "Received connection request from " << ipAddr << " identifying as client " << clientID << " .\n";

		//Skip clients we have already added.
		if (discoveryClient->signalingState == SignalingState::START)
			discoveryClient->signalingState = SignalingState::REQUESTED;
		if (discoveryClient->signalingState != SignalingState::REQUESTED)
			return;
		// if signalingState is START, we should not have a client...
		if (clientManager.hasClient(clientID))
		{
			// ok, we've received a connection request from a client that WE think we already have.
			// Apparently the CLIENT thinks they've disconnected.
			// The client might, as far as we know, have lost the information it needs to continue the connection.
			// THerefore we should resend everything required.
			discoveryClient->signalingState = SignalingState::STREAMING;
			TELEPORT_COUT << "Warning: Client " << clientID << " reconnected, but we didn't know we'd lost them." << std::endl;
			// It may be just that the connection request was already in flight when we accepted its predecessor.
			sendResponseToClient(clientID);
			return;
		}
		//Ignore connections from clients with the wrong IP, if a desired IP has been set.
		if (desiredIP.length() != 0)
		{
			//Create new wide-string with clientIP, and add new client if there is no difference between the new client's IP and the desired IP.
			if (desiredIP.compare(0, ipAddr.size(), { ipAddr.begin(), ipAddr.end() }) == 0)
			{
				discoveryClient->signalingState = SignalingState::ACCEPTED;
			}
		}
		else
		{
			discoveryClient->signalingState = SignalingState::ACCEPTED;
		}
	}
}

// ENet reverses the order of the ENetBuffer struct between unix and Windows.
#ifdef _MSC_VER
#define CREATE_ENET_BUFFER(size,ptr) { size, ptr }
#else
#define CREATE_ENET_BUFFER(size,ptr) { ptr,size}
#endif
void SignalingService::tick()
{
	if (discoveryPort == 0 )
	{
		TELEPORT_INTERNAL_CERR("Attempted to call tick on client discovery service without initalizing!",0);
		return;
	}

	avs::uid clientID = 0; //Newly received ID.
	ENetBuffer buffer = CREATE_ENET_BUFFER(sizeof(clientID), &clientID ); //Buffer to retrieve client ID with.

	ENetAddress addr;
	//Retrieve all packets received since last call, and add any new clients.
	while (size_t packetsize = enet_socket_receive(discoverySocket, &addr, &buffer, 1) > 0)
	{
		//Retrieve IP of client that sent message, and convert to string.
		char clientIPRaw[20];
		enet_address_get_host_ip(&addr, clientIPRaw, 20);
		TELEPORT_COUT << "Received connection request from " << clientIPRaw << " identifying as client "<<clientID<<" .\n";
		bool ipConnecting = false;
		for (const auto& client : newClients)
		{
			if (client.second.host == addr.host)
			{
				ipConnecting = true;
				break;
			}
		}
		if (ipConnecting)
		{
			continue;
		}
		for (const auto& svc : clientManager.clients)
		{
			if (svc.second->eNetAddress.host == addr.host)
			{
				clientID = svc.first;
			}
		}

		if (clientID == 0)
		{
			clientID = TeleportUtility::GenerateID();
		}
		else
		{
			//Skip clients we have already added.
			if (newClients.find(clientID) != newClients.end())
				continue;
			auto s = clientManager.clients.find(clientID);
			if (s != clientManager.clients.end())
			{
				// ok, we've received a connection request from a client that WE think we already have.
				// Apparently the CLIENT thinks they've disconnected.
				TELEPORT_COUT << "Warning: Client " << clientID << " reconnected, but we didn't know we'd lost them."<<std::endl;
				// It may be just that the connection request was already in flight when we accepted its predecessor.
				sendResponseToClient(clientID);
				continue;
			}
		}
		
		//Ignore connections from clients with the wrong IP, if a desired IP has been set.
		if (desiredIP.length() != 0)
		{
			enet_address_get_host_ip(&addr, clientIPRaw, 20);

			//Trying to use the pointer to the string's data results in an incorrect size, and incorrect iterators.
			std::string clientIP = clientIPRaw;

			//Create new wide-stringk with clientIP, and add new client if there is no difference between the new client's IP and the desired IP.
			if (desiredIP.compare(0, clientIP.size(), { clientIP.begin(), clientIP.end() }) == 0)
			{
				newClients[clientID] = addr;
			}
		}
		else
		{
			newClients[clientID] = addr;
		}
	}

	for(auto c=newClients.cbegin(); c!=newClients.cend();)
	{
		auto clientID = c->first;
		auto addr = c->second;
		char clientIP[20];
		enet_address_get_host_ip(&addr, clientIP, sizeof(clientIP));
		if(Client_StartSession(clientID, std::string(clientIP), addr.port))
		{
			c = newClients.erase(c);
		}
		else
		{
			++c;
		}
	}
}

const std::set<avs::uid> &SignalingService::getClientIds() const
{
	return clientUids;
}

std::shared_ptr<SignalingClient> SignalingService::getSignalingClient(avs::uid u)
{
	return signalingClients[u];
}

void SignalingService::sendResponseToClient(uint64_t clientID)
{
	if(discoveryPort == 0)
	{
		TELEPORT_CERR<<"Attempted to call sendResponseToClient on client discovery service without initalising!\n";
		return;
	}

	auto clientPair = clientManager.clients.find(clientID);
	if(clientPair == clientManager.clients.end())
	{
		TELEPORT_CERR << "No client with ID: " << clientID << " is trying to connect.\n";
		return;
	}

	// Send response, containing port to connect on, to all clients we want to host.
	teleport::core::ServiceDiscoveryResponse response = {clientID, servicePort};
	ENetBuffer buffer = CREATE_ENET_BUFFER(sizeof(response), &response );
	TELEPORT_COUT << "Sending server discovery response to client ID: " << clientID << std::endl;
	enet_socket_send(discoverySocket, &clientPair->second->eNetAddress, &buffer, 1);
}

void SignalingService::sendToClient(avs::uid clientID, std::string str)
{
	if (discoveryPort == 0)
	{
		TELEPORT_CERR << "Attempted to call sendResponseToClient on client discovery service without initalising!\n";
		return;
	}

	auto c = signalingClients.find(clientID);
	if (c == signalingClients.end())
	{
		TELEPORT_CERR << "No client with ID: " << clientID << " is trying to connect.\n";
		return;
	}
	try
	{
		signalingClients[clientID]->webSocket->send(str);
		TELEPORT_CERR << "webSocket->send: " << str << "  .\n";
	}
	catch (...)
	{
	}
}

void SignalingService::discoveryCompleteForClient(uint64_t clientID)
{
	auto i = clientManager.clients.find(clientID);
	if (i == clientManager.clients.end())
	{
		
	}
	else
	{
		if (i->second->GetConnectionState() == CONNECTED)
			return;
		i->second->SetConnectionState(CONNECTED);
		AddUnlinkedClientID(clientID);
	}
}