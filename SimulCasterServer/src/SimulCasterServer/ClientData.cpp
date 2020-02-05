#include "SimulCasterServer/ClientData.h"


void ClientData::setOrigin(avs::vec3 pos)
{
	if(clientMessaging.hasPeer())
	{
		clientMessaging.setPosition(pos);
		// ASSUME the message was received...
		_hasOrigin=true;
	}
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