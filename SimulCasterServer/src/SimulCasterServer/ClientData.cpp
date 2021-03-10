#include "SimulCasterServer/ClientData.h"

ClientData::ClientData(std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService, std::shared_ptr<PluginVideoEncodePipeline> videoPipeline, std::shared_ptr<PluginAudioEncodePipeline> audioPipeline, const SCServer::ClientMessaging& clientMessaging)
	: geometryStreamingService(geometryStreamingService), videoEncodePipeline(videoPipeline), audioEncodePipeline(audioPipeline), clientMessaging(clientMessaging)
{
	originClientHas.x = originClientHas.y = originClientHas.z = 0.f;
}

bool ClientData::setOrigin(uint64_t ctr,avs::vec3 pos,bool set_rel,avs::vec3 rel_to_head,avs::vec4 orientation)
{
	if(clientMessaging.hasPeer()&& clientMessaging.hasReceivedHandshake())
	{
		if(clientMessaging.setPosition(ctr,pos,set_rel,rel_to_head,orientation))
		{
		// ASSUME the message was received...
		// TODO: Only set this when client confirms.
			_hasOrigin=true;
			originClientHas=pos;
			return true;
		}
	}
	return false;
}

bool ClientData::isConnected() const
{
	return clientMessaging.hasPeer();
}

bool ClientData::hasOrigin() const
{
	if(clientMessaging.hasPeer())
	{
		return _hasOrigin;
	}
	_hasOrigin=false;
	return false;
}

avs::vec3 ClientData::getOrigin() const
{
	return originClientHas;
}