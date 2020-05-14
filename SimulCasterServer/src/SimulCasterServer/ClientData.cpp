#include "SimulCasterServer/ClientData.h"

bool ClientData::setOrigin(avs::vec3 pos)
{
	if(clientMessaging.hasPeer()&& clientMessaging.hasReceivedHandshake())
	{
		if(clientMessaging.setPosition(pos))
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