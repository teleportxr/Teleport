#pragma once

#include <unordered_map>
#include <set>

#include "libavstream/geometry/mesh_interface.hpp"

namespace SCServer
{
	class GeometryStore;

	class GeometryRequester: public avs::GeometryRequesterBackendInterface
	{
	public:
		float confirmationWaitTime;

		virtual ~GeometryRequester() = default;

		virtual bool hasResource(avs::uid resourceID) const override;

		virtual void encodedResource(avs::uid resourceID) override;
		virtual void requestResource(avs::uid resourceID) override;
		virtual void confirmResource(avs::uid resourceID) override;

		virtual void getResourcesToStream(std::vector<avs::MeshNodeResources>& outMeshResources, std::vector<avs::LightNodeResources>& outLightResources) override;

		void initialise(GeometryStore* geometryStore);

		void startStreamingActor(avs::uid actorID);
		void stopStreamingActor(avs::uid actorID);

		//Adds the hand actors to the list of streamed actors.
		void addHandsToStream();

		void tick(float deltaTime);

		void reset();
	private:
		GeometryStore* geometryStore;

		std::unordered_map<avs::uid, bool> sentResources; //Tracks the resources sent to the user; <resource identifier, doesClientHave>.
		std::unordered_map<avs::uid, float> unconfirmedResourceTimes; //Tracks time since an unconfirmed resource was sent; <resource identifier, time since sent>.

		std::set<avs::uid> streamedActors;

		//Recursively obtains the resources from the mesh node, and its child nodes.
		void GetMeshNodeResources(avs::uid node_uid, std::vector<avs::MeshNodeResources>& outMeshResources);
	};
}