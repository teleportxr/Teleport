#include "PCDiscoveryService.h"

#include "crossplatform/Log.h"
#include "TeleportCore/ErrorHandling.h"

PCDiscoveryService::PCDiscoveryService(uint32_t manualClientID)
	: DiscoveryService(manualClientID)
{
	
}

PCDiscoveryService::~PCDiscoveryService()
{
	if(serviceDiscoverySocket)
	{
		enet_socket_destroy(serviceDiscoverySocket);
		serviceDiscoverySocket = 0;
	}
}

ENetSocket PCDiscoveryService::CreateDiscoverySocket(std::string ip, uint16_t discoveryPort)
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
		TELEPORT_CLIENT_FAIL("Failed to bind to service discovery UDP socket");
		enet_socket_destroy(socket);
		socket = 0;
		return 0;
	}
	return socket;
}

uint32_t PCDiscoveryService::Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string ip, uint16_t serverDiscoveryPort, ENetAddress& remote)
{
	bool serverDiscovered = false;

	if (ip.empty())
	{
		ip = "255.255.255.255";
	}
	if (serverAddress.port != serverDiscoveryPort || serverAddress.host == ENET_HOST_ANY || ip != serverIP)
	{
		serverIP = ip;
		if (!awaiting)
		{
			serverAddress.host = ENET_HOST_ANY;
			serverAddress.port = serverDiscoveryPort;
			fobj = std::async(&enet_address_set_host, &(serverAddress), ip.c_str());
			awaiting = true;
		}
		else
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
					int err = WSAGetLastError();
					TELEPORT_CERR << "enet_address_set_host failed with error " << err << std::endl;
					return 0;
				}
				serverIP = ip;
			}
		}
	}
	if(!serviceDiscoverySocket)
	{
		serviceDiscoverySocket = CreateDiscoverySocket(clientIP, clientDiscoveryPort);
	}
	ENetBuffer buffer = {sizeof(clientID) ,(void*)&clientID};
	avs::ServiceDiscoveryResponse response = {};
	ENetAddress  responseAddress = {0xffffffff, 0};
	ENetBuffer responseBuffer = {sizeof(response),&response};
	// Send our client id to the server on the discovery port. Once every 1000 frames.
	static int frame=1;
	frame--;
	if(!frame)
	{
		frame = 1000;
		int res = enet_socket_send(serviceDiscoverySocket, &serverAddress, &buffer, 1);
		if(res==-1)
		{
			int err=WSAGetLastError();
			TELEPORT_CERR<<"PCDicoveryService enet_socket_send failed with error "<<err<<std::endl;
			return 0;
		}
	}

	static size_t bytesRecv;
	do
	{
		// This will change responseAddress from 0xffffffff into the address of the server
		bytesRecv = enet_socket_receive(serviceDiscoverySocket, &responseAddress, &responseBuffer, 1);
		if(bytesRecv == sizeof(response) && clientID == response.clientID)
		{
			remote.host = responseAddress.host;
			remote.port = response.remotePort;
			serverDiscovered = true;
		}
	}
	while(bytesRecv > 0 && !serverDiscovered);

	if(serverDiscovered)
	{
		char remoteIP[20];
		enet_address_get_host_ip(&remote, remoteIP, sizeof(remoteIP));
		TELEPORT_CLIENT_LOG("Discovered session server: %s:%d", remoteIP, remote.port);

		enet_socket_destroy(serviceDiscoverySocket);
		serviceDiscoverySocket = 0;
		return clientID;
	}
	return 0;
}