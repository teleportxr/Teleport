#include "SimulCasterServer/ClientData.h"
using namespace teleport;
using namespace server;

ClientData::ClientData(std::shared_ptr<teleport::GeometryStreamingService> geometryStreamingService, std::shared_ptr<PluginVideoEncodePipeline> videoPipeline, std::shared_ptr<PluginAudioEncodePipeline> audioPipeline, const teleport::ClientMessaging& clientMessaging)
	: geometryStreamingService(geometryStreamingService), videoEncodePipeline(videoPipeline), audioEncodePipeline(audioPipeline), clientMessaging(clientMessaging)
{
	originClientHas.x = originClientHas.y = originClientHas.z = 0.f;
	memset(&clientSettings,0,sizeof(clientSettings));
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

void ClientData::setGlobalIlluminationTextures(size_t num,const avs::uid *uids)
{
	if(num>255)
	{
		num=255;
		TELEPORT_CERR<<"Too many GI Textures."<<std::endl;
	}
	if(global_illumination_texture_uids.size()!=num)
		global_illumination_texture_uids.resize(num);
	bool changed=false;
	for(size_t i=0;i<num;i++)
	{
		if(global_illumination_texture_uids[i]!=uids[i])
		{
			changed=true;
			global_illumination_texture_uids[i] = uids[i];
			geometryStreamingService->addGenericTexture(uids[i]);
		}
	}
	if (!isStreaming)
		return;
	if(changed)
	{
		avs::SetupLightingCommand setupLightingCommand;
		setupLightingCommand.num_gi_textures=(uint8_t)num;
		clientMessaging.sendCommand(std::move(setupLightingCommand), global_illumination_texture_uids);
	}
	
}