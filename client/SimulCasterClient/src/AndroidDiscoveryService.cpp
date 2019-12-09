#include "AndroidDiscoveryService.h"

#include <ctime>

#include "crossplatform/Log.h"

struct ServiceDiscoveryResponse {
    uint32_t clientID;
    uint16_t remotePort;
} __attribute__((packed));

AndroidDiscoveryService::AndroidDiscoveryService()
{
    struct timespec timeNow;
    clock_gettime(CLOCK_REALTIME, &timeNow);

    // Generate random client ID
    const unsigned int timeNowMs = static_cast<unsigned int>(timeNow.tv_sec * 1000 + timeNow.tv_nsec / 1000000);
    srand(timeNowMs);
    clientID = static_cast<uint32_t>(rand());
}

AndroidDiscoveryService::~AndroidDiscoveryService()
{
    if(serviceDiscoverySocket)
    {
        close(serviceDiscoverySocket);
    }
}

bool AndroidDiscoveryService::Discover(uint16_t discoveryPort, ENetAddress& remote)
{
    bool serverDiscovered = false;

    struct sockaddr_in broadcastAddress = { AF_INET, htons(discoveryPort) };
    broadcastAddress.sin_addr.s_addr = INADDR_BROADCAST;

    if(!serviceDiscoverySocket) {
        serviceDiscoverySocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(serviceDiscoverySocket <= 0) {
            FAIL("Failed to create service discovery UDP socket");
            return false;
        }

        int flagEnable = 1;
        setsockopt(serviceDiscoverySocket, SOL_SOCKET, SO_REUSEADDR, &flagEnable, sizeof(int));
        setsockopt(serviceDiscoverySocket, SOL_SOCKET, SO_BROADCAST, &flagEnable, sizeof(int));

        struct sockaddr_in bindAddress = { AF_INET, htons(discoveryPort) };
        if(bind(serviceDiscoverySocket, (struct sockaddr*)&bindAddress, sizeof(bindAddress)) == -1) {
            FAIL("Failed to bind to service discovery UDP socket");
            close(serviceDiscoverySocket);
            serviceDiscoverySocket = 0;
            return false;
        }
    }

    sendto(serviceDiscoverySocket, &clientID, sizeof(clientID), 0,
           (struct sockaddr*)&broadcastAddress, sizeof(broadcastAddress));

    {
        ServiceDiscoveryResponse response = {};
        struct sockaddr_in responseAddr;
        socklen_t responseAddrSize = sizeof(responseAddr);

        ssize_t bytesRecv;
        do {
            bytesRecv = recvfrom(serviceDiscoverySocket, &response, sizeof(response),
                                 MSG_DONTWAIT,
                                 (struct sockaddr*)&responseAddr, &responseAddrSize);

            if(bytesRecv == sizeof(response) && clientID == response.clientID) {
                remote.host = responseAddr.sin_addr.s_addr;
                remote.port = response.remotePort;
                serverDiscovered = true;
            }
        } while(bytesRecv > 0 && !serverDiscovered);
    }

    if(serverDiscovered) {
        char remoteIP[20];
        enet_address_get_host_ip(&remote, remoteIP, sizeof(remoteIP));
        WARN("Discovered session server: %s:%d", remoteIP, remote.port);

        close(serviceDiscoverySocket);
        serviceDiscoverySocket = 0;
    }
    return serverDiscovered;
}