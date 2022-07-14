#include "DefaultDiscoveryService.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportServer/ClientData.h"
#include "TeleportServer/ServerSettings.h"    
#include "TeleportUtility.h"

using namespace teleport;
using namespace server;

extern std::map<avs::uid, ClientData> clientServices;
extern teleport::ServerSettings casterSettings;
TELEPORT_EXPORT bool Client_StartSession(avs::uid clientID, std::string clientIP);
TELEPORT_EXPORT void AddUnlinkedClientID(avs::uid clientID);

bool DefaultDiscoveryService::initialize(uint16_t discovPort, uint16_t servPort, std::string desIP)
{
	if (discovPort != 0)
		discoveryPort = discovPort;

	if (discoveryPort == 0)
	{
		TELEPORT_CERR <<"Discovery port is not set.\n";
		return false;
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
		int err= WSAGetLastError();
		TELEPORT_CERR << "Failed to bind discovery socket on port: " << address.port << " with error "<<err<<std::endl;
		enet_socket_destroy(discoverySocket);
		discoverySocket = 0;
		return false;
	}

	return true;
}

void DefaultDiscoveryService::shutdown()
{
	enet_socket_destroy(discoverySocket);
	discoverySocket = 0;
	newClients.clear();
}

void DefaultDiscoveryService::tick()
{
	if (!discoverySocket || discoveryPort == 0 || servicePort == 0)
	{
		printf_s("Attempted to call tick on client discovery service without initalizing!");
		return;
	}

	avs::uid clientID = 0; //Newly received ID.
	ENetBuffer buffer = { sizeof(clientID), &clientID }; //Buffer to retrieve client ID with.

	ENetAddress addr;
	//Retrieve all packets received since last call, and add any new clients.
	while (size_t packetsize = enet_socket_receive(discoverySocket, &addr, &buffer, 1) > 0)
	{
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

		if (clientID == 0)
		{
			clientID = TeleportUtility::GenerateID();
		}
		else
		{
			//Skip clients we have already added.
			if (newClients.find(clientID) != newClients.end())
				continue;

			if (clientServices.find(clientID) != clientServices.end())
			{
				// ok, we've received a connection request from a client that WE think we already have.
				// Apparently the CLIENT thinks they've disconnected.
				TELEPORT_COUT << "Warning: Client " << clientID << " reconnected, but we didn't know we'd lost them.\n";
			}
		}
		
		//Ignore connections from clients with the wrong IP, if a desired IP has been set.
		if (desiredIP.length() != 0)
		{
			//Retrieve IP of client that sent message, and convert to string.
			char clientIPRaw[20];
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

	for (auto c = newClients.cbegin(); c != newClients.cend();)
	{
		auto clientID = c->first;
		auto addr = c->second;

		char clientIP[20];
		enet_address_get_host_ip(&addr, clientIP, sizeof(clientIP));

		if(Client_StartSession(clientID, std::string(clientIP)))
		{
			++c;
		}
		else
		{
			c = newClients.erase(c);
		}
	}
}

void DefaultDiscoveryService::sendResponseToClient(uint64_t clientID)
{
	if(!discoverySocket || discoveryPort == 0 || servicePort == 0)
	{
		printf_s("Attempted to call sendResponseToClient on client discovery service without initalising!");
		return;
	}

	auto clientPair = newClients.find(clientID);
	if(clientPair == newClients.end())
	{
		TELEPORT_CERR << "No client with ID: " << clientID << " is trying to connect.\n";
		return;
	}

	// Send response, containing port to connect on, to all clients we want to host.
	ENetAddress addr = clientPair->second;
	avs::ServiceDiscoveryResponse response = {clientID, servicePort};
	ENetBuffer buffer = {sizeof(response), &response};
	enet_socket_send(discoverySocket, &addr, &buffer, 1);
}

void DefaultDiscoveryService::discoveryCompleteForClient(uint64_t clientID)
{
	auto i = newClients.find(clientID);
	if (i == newClients.end())
	{
		TELEPORT_CERR << "Client had already completed discovery\n";
	}
	else
	{
		newClients.erase(i);
		AddUnlinkedClientID(clientID);
	}
}