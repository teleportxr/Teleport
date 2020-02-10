#include "libavstream/common.hpp"
#include "SimulCasterServer/ClientMessaging.h"
#include "SimulCasterServer/GeometryStreamingService.h"

class PluginDiscoveryService;
class PluginGeometryStreamingService;


class ClientData
{
public:
	ClientData(std::shared_ptr<PluginGeometryStreamingService> gs, std::function<void(void)> disconnect);
	SCServer::CasterContext casterContext;

	std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService;
	SCServer::ClientMessaging clientMessaging;

	bool isStreaming = false;

	void setOrigin(avs::vec3 pos);
	bool isConnected() const;
	bool hasOrigin() const;
protected:
	mutable bool _hasOrigin=false;
};
