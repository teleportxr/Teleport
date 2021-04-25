#include "DefaultDiscoveryService.h"

#include "SimulCasterServer/ErrorHandling.h"
#include "SimulCasterServer/ClientData.h"
#include "SimulCasterServer/CasterSettings.h"
extern std::map<avs::uid, ClientData> clientServices;
extern SCServer::CasterSettings casterSettings;
TELEPORT_EXPORT void Client_StartSession(avs::uid clientID, int32_t listenPort);

using namespace SCServer;

bool DefaultDiscoveryService::initialise(uint16_t discovPort, uint16_t servPort)
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

	discoverySocket = enet_socket_create(ENetSocketType::ENET_SOCKET_TYPE_DATAGRAM);
	if (discoverySocket <= 0)
	{
		TELEPORT_CERR <<"Failed to create discovery socket.\n";
		return false;
	}

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
		printf_s("Attempted to call tick on client discovery service without initalising!");
		return;
	}

	uint32_t clientID = 0; //Newly received ID.
	ENetBuffer buffer = { sizeof(clientID), &clientID }; //Buffer to retrieve client ID with.
	ENetAddress addr;
	//Retrieve all packets received since last call, and add any new clients.
	while (size_t packetsize = enet_socket_receive(discoverySocket, &addr, &buffer, 1) > 0)
	{
		//Skip clients we have already added.
		if (newClients.find(clientID) != newClients.end())
			continue;

		bool already_got = false;
		if (clientServices.find(clientID) != clientServices.end())
		{
			// ok, we've received a connection request from a client that WE think we already have.
			// Apparently the CLIENT thinks they've disconnected.
			TELEPORT_COUT << "Warning: Client "<<clientID<<" reconnected, but we didn't know we'd lost them.\n";
			already_got=true;
		}
		
		std::wstring desiredIP(casterSettings.clientIP);
		//Ignore connections from clients with the wrong IP, if a desired IP has been set.
		if (desiredIP.length() != 0)
		{
			//Retrieve IP of client that sent message, and covert to string.
			char clientIPRaw[20];
			enet_address_get_host_ip(&addr, clientIPRaw, 20);

			//Trying to use the pointer to the string's data results in an incorrect size, and incorrect iterators.
			std::string clientIP = clientIPRaw;

			//Create new wide-stringk with clientIP, and add new client if there is no difference between the new client's IP and the desired IP.
			if (desiredIP.compare(0, clientIP.size(), { clientIP.begin(), clientIP.end() }) == 0)
			{
				newClients[clientID] = addr;
			}
			else if(already_got)
			{
				//we should remove this client because its IP is wrong
				TELEPORT_CERR << "But "<<clientID<<" has the wrong IP, should drop.\n";
			}
		}
		else
		{
			newClients[clientID] = addr;
		}
	}

	//Send response, containing port to connect on, to all clients we want to host.
	for (auto& c : newClients)
	{
		auto clientID = c.first;
		auto addr = c.second;
		ServiceDiscoveryResponse response = { clientID, servicePort };

		buffer = { sizeof(ServiceDiscoveryResponse), &response };
		enet_socket_send(discoverySocket, &addr, &buffer, 1);
		Client_StartSession(clientID, servicePort);
	}
}

void DefaultDiscoveryService::discoveryCompleteForClient(uint64_t ClientID)
{
	auto i = newClients.find(ClientID);
	if (i == newClients.end())
	{
		TELEPORT_CERR << "Client had already completed discovery\n";
	}
	else
	{
		newClients.erase(i);
	}
}