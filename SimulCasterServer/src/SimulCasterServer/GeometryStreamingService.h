#pragma once

#include <unordered_map>
#include <set>

#include "libavstream/geometry/mesh_interface.hpp"
#include "libavstream/pipeline.hpp"
#include "libavstream/common_networking.h"

#include "CasterContext.h"
#include "GeometryEncoder.h"

namespace teleport
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

		virtual void getResourcesToStream(std::vector<avs::uid>& outNodeIDs
		, std::vector<avs::MeshNodeResources>& outMeshResources
		, std::vector<avs::LightNodeResources>& outLightResources
		,std::set<avs::uid>& genericTextureUids,int32_t minimumPriority) const override;

		virtual avs::AxesStandard getClientAxesStandard() const override
		{
			return casterContext->axesStandard;
		}

		virtual void startStreaming(CasterContext* context,const avs::Handshake &handshake);
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


		void addGenericTexture(avs::uid id);
	protected:
		GeometryStore* geometryStore = nullptr;

		virtual bool showNode_Internal(avs::uid clientID, avs::uid nodeID) = 0;
		virtual bool hideNode_Internal(avs::uid clientID, avs::uid nodeID) = 0;
		avs::Handshake handshake;
	private:
		const struct CasterSettings* settings;

		teleport::CasterContext* casterContext = nullptr;
		teleport::GeometryEncoder geometryEncoder;

		// The following MIGHT be moved later to a separate Pipeline class:
		std::unique_ptr<avs::Pipeline> avsPipeline;
		std::unique_ptr<avs::GeometrySource> avsGeometrySource;
		std::unique_ptr<avs::GeometryEncoder> avsGeometryEncoder;

		std::unordered_map<avs::uid, bool> sentResources; //Tracks the resources sent to the user; <resource identifier, doesClientHave>.
		std::unordered_map<avs::uid, float> unconfirmedResourceTimes; //Tracks time since an unconfirmed resource was sent; <resource identifier, time since sent>.
		std::set<avs::uid> streamedNodeIDs; //Nodes that the client needs to draw, and should be sent to them.
		std::set<avs::uid> hiddenNodes; //Nodes that are currently hidden on the server.
		std::set<avs::uid> streamedGenericTextureUids; // Textures that are not specifically specified in a material, e.g. lightmaps.

		//Recursively obtains the resources from the mesh node, and its child nodes.
		void GetMeshNodeResources(avs::uid nodeID, const avs::Node& node, std::vector<avs::MeshNodeResources>& outMeshResources, int32_t minimumPriority) const;
	};
}