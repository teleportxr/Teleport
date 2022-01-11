#include "AndroidDiscoveryService.h"

#include <random>

#include "libavstream/common_networking.h"

#include "TeleportClient/Log.h"

#include <arpa/inet.h>

AndroidDiscoveryService::AndroidDiscoveryService()
{

}

AndroidDiscoveryService::~AndroidDiscoveryService()
{
    if(serviceDiscoverySocket)
    {
        enet_socket_destroy(serviceDiscoverySocket);
        serviceDiscoverySocket = 0;
    }
}

uint32_t AndroidDiscoveryService::Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote)
{
    bool serverDiscovered = false;

    if (serverIP.empty())
    {
        serverIP = "255.255.255.255";
    }

    struct sockaddr_in serverAddress = { AF_INET, htons(serverDiscoveryPort) };
    serverAddress.sin_addr.s_addr = inet_addr(serverIP.c_str());

    if(!serviceDiscoverySocket)
    {
        serviceDiscoverySocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(serviceDiscoverySocket <= 0)
        {
            TELEPORT_CLIENT_FAIL("Failed to create service discovery UDP socket");
            return 0;
        }

        int flagEnable = 1;
        setsockopt(serviceDiscoverySocket, SOL_SOCKET, SO_REUSEADDR, &flagEnable, sizeof(int));
        setsockopt(serviceDiscoverySocket, SOL_SOCKET, SO_BROADCAST, &flagEnable, sizeof(int));
        setsockopt(serviceDiscoverySocket, SOL_SOCKET, SO_DEBUG, &flagEnable, sizeof(int));

        struct sockaddr_in bindAddress = { AF_INET, htons(clientDiscoveryPort) };
        if(bind(serviceDiscoverySocket, (struct sockaddr*)&bindAddress, sizeof(bindAddress)) == -1)
        {
            enet_socket_destroy(serviceDiscoverySocket);
            serviceDiscoverySocket = 0;

            TELEPORT_CLIENT_FAIL("Failed to bind to service discovery UDP socket");
            return 0;
        }
    }
    // Don't send too many broadcasts.
    static uint8_t counter=60;
    counter--;
    if(!counter)
    {
        sendto(serviceDiscoverySocket, &clientID, sizeof(clientID), 0,
               (struct sockaddr *) & serverAddress, sizeof(serverAddress));
        counter = 60;
    }

    {
        avs::ServiceDiscoveryResponse response = {};
        struct sockaddr_in responseAddr;
        socklen_t responseAddrSize = sizeof(responseAddr);

        ssize_t bytesRecv;
        do
        {
            bytesRecv=recvfrom(serviceDiscoverySocket, &response, sizeof(response), MSG_DONTWAIT
                               , (struct sockaddr *) &responseAddr, &responseAddrSize);
            // we'll receive a lot of 4-byte packets, our own broadcast that we've just sent...
            if(bytesRecv==sizeof(response))
            {
                remote.host=responseAddr.sin_addr.s_addr;
                remote.port=response.remotePort;
                serverDiscovered=true;
                clientID = response.clientID;
                //sendto(serviceDiscoverySocket, &remote.port, sizeof(remote.port) , 0, (struct sockaddr *) &responseAddr, sizeof(responseAddr));
            }
            else if(errno!=0&&errno!=11)
            {
                TELEPORT_CLIENT_WARN("recvfrom %d",errno);
            }
        } while(bytesRecv > 0 && !serverDiscovered);
    }

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