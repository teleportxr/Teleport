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

	class GeometryStreamingService : public avs::GeometryRequesterBackendInterface
	{
	public:
		GeometryStreamingService(const struct CasterSettings* settings);
		virtual ~GeometryStreamingService();

		virtual bool hasResource(avs::uid resourceID) const override;

		virtual void encodedResource(avs::uid resourceID) override;
		virtual void requestResource(avs::uid resourceID) override;
		virtual void confirmResource(avs::uid resourceID) override;

		virtual void getResourcesToStream(std::vector<avs::uid>& outNodeIDs, std::vector<avs::MeshNodeResources>& outMeshResources, std::vector<avs::LightNodeResources>& outLightResources) const override;

		virtual avs::AxesStandard getClientAxesStandard() const override
		{
			return casterContext->axesStandard;
		}

		virtual void startStreaming(CasterContext* context);
		//Stop streaming to client.
		virtual void stopStreaming();

		void hideNode(avs::uid clientID, avs::uid nodeID);
		void showNode(avs::uid clientID, avs::uid nodeID);
		void setNodeVisible(avs::uid clientID, avs::uid nodeID, bool isVisible);
		bool isClientRenderingNode(avs::uid nodeID);

		virtual void tick(float deltaTime);

		virtual void reset();

		const std::set<avs::uid>& getStreamedNodeIDs()
		{
			return streamedNodeIDs;
		}

		void addNode(avs::uid nodeID);
		void removeNode(avs::uid nodeID);
		bool isStreamingNode(avs::uid nodeID);
	protected:
		GeometryStore* geometryStore = nullptr;

		virtual void showNode_Internal(avs::uid clientID, avs::uid nodeID) = 0;
		virtual void hideNode_Internal(avs::uid clientID, avs::uid nodeID) = 0;

	private:
		const struct CasterSettings* settings;

		SCServer::CasterContext* casterContext = nullptr;
		SCServer::GeometryEncoder geometryEncoder;

		// The following MIGHT be moved later to a separate Pipeline class:
		std::unique_ptr<avs::Pipeline> avsPipeline;
		std::unique_ptr<avs::GeometrySource> avsGeometrySource;
		std::unique_ptr<avs::GeometryEncoder> avsGeometryEncoder;

		std::unordered_map<avs::uid, bool> sentResources; //Tracks the resources sent to the user; <resource identifier, doesClientHave>.
		std::unordered_map<avs::uid, float> unconfirmedResourceTimes; //Tracks time since an unconfirmed resource was sent; <resource identifier, time since sent>.
		std::set<avs::uid> streamedNodeIDs; //Nodes that the client needs to draw, and should be sent to them.
		std::set<avs::uid> hiddenNodes; //Nodes that are currently hidden on the server.

		//Recursively obtains the resources from the mesh node, and its child nodes.
		void GetMeshNodeResources(avs::uid nodeID, const avs::DataNode& node, std::vector<avs::MeshNodeResources>& outMeshResources) const;
	};
}