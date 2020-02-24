#include "libavstream/common.hpp"
#include "SimulCasterServer/ClientMessaging.h"
#include "SimulCasterServer/GeometryStreamingService.h"
#include "SimulCasterServer/VideoEncodePipeline.h"

class PluginDiscoveryService;
class PluginGeometryStreamingService;
class PluginVideoEncodePipeline;

class ClientData
{
public:
	ClientData(std::shared_ptr<PluginGeometryStreamingService> gs, std::shared_ptr<PluginVideoEncodePipeline> vep, std::function<void(void)> disconnect);
	SCServer::CasterContext casterContext;

	std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService;
	std::shared_ptr<PluginVideoEncodePipeline> videoEncodePipeline;
	SCServer::ClientMessaging clientMessaging;

	bool isStreaming = false;
	bool videoKeyframeRequired = false;

	void setOrigin(avs::vec3 pos);
	bool isConnected() const;
	bool hasOrigin() const;
protected:
	mutable bool _hasOrigin=false;
};
