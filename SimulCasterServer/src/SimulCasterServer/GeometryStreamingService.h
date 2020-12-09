#pragma once

#include <unordered_map>
#include <set>

#include "libavstream/geometry/mesh_interface.hpp"
#include "libavstream/pipeline.hpp"

#include "CasterContext.h"
#include "GeometryEncoder.h"

namespace SCServer
{
	class GeometryStore;

	class GeometryStreamingService: public avs::GeometryRequesterBackendInterface
	{
	public:
		GeometryStreamingService(const struct CasterSettings* settings);
		virtual ~GeometryStreamingService();

		virtual bool hasResource(avs::uid resourceID) const override;

		virtual void encodedResource(avs::uid resourceID) override;
		virtual void requestResource(avs::uid resourceID) override;
		virtual void confirmResource(avs::uid resourceID) override;

		virtual void getResourcesToStream(std::vector<avs::MeshNodeResources>& outMeshResources, std::vector<avs::LightNodeResources>& outLightResources) const override;

		virtual avs::AxesStandard getAxesStandard() const override
		{
			return casterContext->axesStandard;
		}

		virtual void startStreaming(CasterContext* context);
		//Stop streaming to client.
		virtual void stopStreaming();

		void hideNode(avs::uid clientID,avs::uid nodeID);
		void showNode(avs::uid clientID,avs::uid nodeID);
		void setNodeVisible(avs::uid clientID,avs::uid nodeID, bool isVisible);
		bool isClientRenderingNode(avs::uid nodeID);

		//Adds the hand actors to the list of streamed actors.
		void addHandsToStream();

		virtual void tick(float deltaTime);

		virtual void reset();

		std::vector<avs::uid> getStreameNodeIDs();

		void addNode(avs::uid nodeID);
		avs::uid removeNodeByID(avs::uid nodeID);
		bool isStreamingNodeID(avs::uid nodeID);
	protected:
		GeometryStore* geometryStore;

		virtual void showActor_Internal(avs::uid clientID,avs::uid nodeID) = 0;
		virtual void hideActor_Internal(avs::uid clientID,avs::uid nodeID) = 0;

	private:
		const struct CasterSettings* settings;		

		std::unordered_map<avs::uid, bool> sentResources; //Tracks the resources sent to the user; <resource identifier, doesClientHave>.
		std::unordered_map<avs::uid, float> unconfirmedResourceTimes; //Tracks time since an unconfirmed resource was sent; <resource identifier, time since sent>.

		SCServer::CasterContext* casterContext;
		SCServer::GeometryEncoder geometryEncoder;

		// The following MIGHT be moved later to a separate Pipeline class:
		std::unique_ptr<avs::Pipeline> avsPipeline;
		std::unique_ptr<avs::GeometrySource> avsGeometrySource;
		std::unique_ptr<avs::GeometryEncoder> avsGeometryEncoder;
		std::set<avs::uid> streamedNodeUids;				//Nodes that the client needs to draw, and should be sent to them;
		std::set<avs::uid> hiddenNodes;	//Nodes that are currently hidden on the server;

		//Recursively obtains the resources from the mesh node, and its child nodes.
		void GetMeshNodeResources(avs::uid node_uid, std::vector<avs::MeshNodeResources>& outMeshResources) const;
	};
}