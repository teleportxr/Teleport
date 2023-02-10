#include "AndroidDiscoveryService.h"

#include <random>

#include "TeleportCore/CommonNetworking.h"

#include "TeleportClient/Log.h"

#include <arpa/inet.h>
using namespace teleport;
using namespace android;
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
