#pragma once

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "SimulCasterServer/ClientMessaging.h"
#include "SimulCasterServer/GeometryStreamingService.h"
#include "SimulCasterServer/CasterSettings.h"

class PluginDiscoveryService;
class PluginGeometryStreamingService;
class PluginVideoEncodePipeline;
class PluginAudioEncodePipeline;

class ClientData
{
public:
	ClientData(std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService, std::shared_ptr<PluginVideoEncodePipeline> videoPipeline, std::shared_ptr<PluginAudioEncodePipeline> audioPipeline, const SCServer::ClientMessaging& clientMessaging);
	
	// client settings from engine-side:
	SCServer::ClientSettings clientSettings;
	SCServer::CasterContext casterContext;

	std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService;
	std::shared_ptr<PluginVideoEncodePipeline> videoEncodePipeline;
	std::shared_ptr<PluginAudioEncodePipeline> audioEncodePipeline;
	SCServer::ClientMessaging clientMessaging;

	bool isStreaming = false;
	bool validClientSettings = false;
	bool videoKeyframeRequired = false;

	bool setOrigin(uint64_t ctr,avs::vec3 pos,bool set_rel,avs::vec3 rel_to_head,avs::vec4 orientation);
	bool isConnected() const;
	bool hasOrigin() const;
	avs::vec3 getOrigin() const;

protected:
	mutable bool _hasOrigin=false;
	avs::vec3 originClientHas;
	ENetAddress address = {};
};


struct ClientStatus
{
	unsigned ipAddress;
};