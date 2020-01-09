#include "ClientMessaging.h"

using namespace SCServer;

void ClientMessaging::tick(float deltaTime)
{
	//static float timeSinceLastGeometryStream = 0;
	//timeSinceLastGeometryStream += deltaTime;

	//const float TIME_BETWEEN_GEOMETRY_TICKS = 1.0f / Monitor->GeometryTicksPerSecond;

	////Only tick the geometry streaming service a set amount of times per second.
	//if(timeSinceLastGeometryStream >= TIME_BETWEEN_GEOMETRY_TICKS)
	//{
	//	GeometryStreamingService.Tick(TIME_BETWEEN_GEOMETRY_TICKS);

	//	//Tell the client to change the visibility of actors that have changed whether they are within streamable bounds.
	//	if(!ActorsEnteredBounds.empty() || !ActorsLeftBounds.empty())
	//	{
	//		size_t commandSize = sizeof(avs::ActorBoundsCommand);
	//		size_t enteredBoundsSize = sizeof(avs::uid) * ActorsEnteredBounds.size();
	//		size_t leftBoundsSize = sizeof(avs::uid) * ActorsLeftBounds.size();

	//		avs::ActorBoundsCommand boundsCommand(ActorsEnteredBounds.size(), ActorsLeftBounds.size());
	//		ENetPacket* packet = enet_packet_create(&boundsCommand, commandSize, ENET_PACKET_FLAG_RELIABLE);

	//		//Resize packet, and insert actor lists.
	//		enet_packet_resize(packet, commandSize + enteredBoundsSize + leftBoundsSize);
	//		memcpy(packet->data + commandSize, ActorsEnteredBounds.data(), enteredBoundsSize);
	//		memcpy(packet->data + commandSize + enteredBoundsSize, ActorsLeftBounds.data(), leftBoundsSize);

	//		enet_peer_send(ClientPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet);

	//		ActorsEnteredBounds.clear();
	//		ActorsLeftBounds.clear();
	//	}

	//	timeSinceLastGeometryStream -= TIME_BETWEEN_GEOMETRY_TICKS;
	//}
}
