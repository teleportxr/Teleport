#pragma once

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "SimulCasterServer/ClientMessaging.h"
#include "SimulCasterServer/GeometryStreamingService.h"
#include "enet/enet.h"

class PluginDiscoveryService;
class PluginGeometryStreamingService;
class PluginVideoEncodePipeline;
class PluginAudioEncodePipeline;

class ClientData
{
public:
	ClientData(std::shared_ptr<PluginGeometryStreamingService> gs, std::shared_ptr<PluginVideoEncodePipeline> vep, std::shared_ptr<PluginAudioEncodePipeline> aep, std::function<void(void)> disconnect);
	SCServer::CasterContext casterContext;

	std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService;
	std::shared_ptr<PluginVideoEncodePipeline> videoEncodePipeline;
	std::shared_ptr<PluginAudioEncodePipeline> audioEncodePipeline;
	SCServer::ClientMessaging clientMessaging;

	bool isStreaming = false;
	bool videoKeyframeRequired = false;

	bool setOrigin(avs::vec3 pos,bool set_rel,avs::vec3 rel_to_head);
	bool isConnected() const;
	bool hasOrigin() const;
	avs::vec3 getOrigin() const;
protected:
	mutable bool _hasOrigin=false;
	avs::vec3 originClientHas;
	ENetAddress address;
};
