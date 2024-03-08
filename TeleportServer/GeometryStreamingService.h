#pragma once

#include <unordered_map>
#include <set>

#include "libavstream/geometry/mesh_interface.hpp"
#include "libavstream/pipeline.hpp"
#include "TeleportCore/CommonNetworking.h"

#include "ClientNetworkContext.h"
#include "GeometryEncoder.h"
#include "GeometryStore.h"

#if TELEPORT_INTERNAL_CHECKS
#define TELEPORT_DEBUG_NODE_STREAMING 1
#else
#define TELEPORT_DEBUG_NODE_STREAMING 0
#endif
 
namespace teleport
{
	namespace server
	{
		//! This per-client class tracks the resources and nodes that the client needs,
		//! and returns them via GeometryRequesterBackendInterface to the encoder.
		class GeometryStreamingService : public avs::GeometryRequesterBackendInterface
		{
		public:
			GeometryStreamingService( avs::uid clid);
			virtual ~GeometryStreamingService();

			virtual bool hasResource(avs::uid resourceID) const override;

			virtual void encodedResource(avs::uid resourceID) override;
			virtual void requestResource(avs::uid resourceID) override;
			virtual void confirmResource(avs::uid resourceID) override;

			void getResourcesToStream(std::set<avs::uid>& outNodeIDs
				, std::vector<avs::MeshNodeResources>& outMeshResources
				, std::vector<avs::LightNodeResources>& outLightResources
				, std::set<avs::uid>& genericTextureUids
				, std::vector<avs::uid>& textCanvases
				, std::vector<avs::uid>& fontAtlases
				, int32_t minimumPriority) const;

			virtual avs::AxesStandard getClientAxesStandard() const override;
			virtual avs::RenderingFeatures getClientRenderingFeatures() const override;

			virtual void startStreaming(ClientNetworkContext* context, const teleport::core::Handshake& handshake);
			//Stop streaming to client.
			void stopStreaming();

			static void clientStartedRenderingNode(avs::uid clientID, avs::uid nodeID);
			static void clientStoppedRenderingNode(avs::uid clientID, avs::uid nodeID);
			static void setNodeVisible(avs::uid clientID, avs::uid nodeID, bool isVisible);
			bool isClientRenderingNode(avs::uid nodeID);

			virtual void tick(float deltaTime);

			virtual void reset();

			const std::set<avs::uid>& getStreamedNodeIDs()
			{
				return streamedNodeIDs;
			}
			// The origin node for the client - must have this in order to stream geometry.
			void setOriginNode(avs::uid nodeID);
			void addNode(avs::uid nodeID);
			void removeNode(avs::uid nodeID);
			bool isStreamingNode(avs::uid nodeID);

			void addGenericTexture(avs::uid id);
		protected:
			avs::uid originNodeId = 0;
			int32_t priority = 0;
			// The lowest priority for which the client has confirmed all the nodes we sent.
			// We only send lower-priority nodes when all higher priorities have been confirmed.
			int32_t lowest_confirmed_node_priority=-100000;
			// How many nodes we have unconfirmed 
			std::map<int32_t,uint32_t> unconfirmed_priority_counts;
			GeometryStore* geometryStore = nullptr;

			virtual bool clientStoppedRenderingNode_Internal(avs::uid clientID, avs::uid nodeID) = 0;
			virtual bool clientStartedRenderingNode_Internal(avs::uid clientID, avs::uid nodeID) = 0;
			bool startedRenderingNode(avs::uid nodeID);
			bool stoppedRenderingNode(avs::uid nodeID);
			teleport::core::Handshake handshake;

			ClientNetworkContext* clientNetworkContext = nullptr;
			GeometryEncoder geometryEncoder;

			// The following MIGHT be moved later to a separate Pipeline class:
			std::unique_ptr<avs::Pipeline> avsPipeline;
			std::unique_ptr<avs::GeometrySource> avsGeometrySource;
			std::unique_ptr<avs::GeometryEncoder> avsGeometryEncoder;

			std::unordered_map<avs::uid, bool> sentResources; //Tracks the resources sent to the user; <resource identifier, doesClientHave>.
			std::unordered_map<avs::uid, float> unconfirmedResourceTimes; //Tracks time since an unconfirmed resource was sent; <resource identifier, time since sent>.
			 /// Nodes that the client needs to draw, which should be sent to the client if it doesn't have them. Not all of these will always be sent: they're sent in order of priority,
			 ///  and only when the client confirms higher priority nodes are the lower-priority ones sent.
			std::set<avs::uid> streamedNodeIDs;
			std::set<avs::uid> clientRenderingNodes; //Nodes that are currently rendered on this client.
			std::set<avs::uid> streamedGenericTextureUids; // Textures that are not specifically specified in a material, e.g. lightmaps.

			//Recursively obtains the resources from the mesh node, and its child nodes.
			void GetMeshNodeResources(avs::uid nodeID, const avs::Node& node, std::vector<avs::MeshNodeResources>& outMeshResources, int32_t minimumPriority) const;
			void GetSkeletonNodeResources(avs::uid nodeID, const avs::Node &node, std::vector<avs::MeshNodeResources> &outSkeletonNodeResources) const;
			avs::uid clientId = 0;
		};

		typedef bool(TELEPORT_STDCALL* ClientStoppedRenderingNodeFn)(avs::uid clientID, avs::uid nodeID);
		typedef bool(TELEPORT_STDCALL* ClientStartedRenderingNodeFn)(avs::uid clientID, avs::uid nodeID);
		class PluginGeometryStreamingService : public GeometryStreamingService
		{
		public:
			PluginGeometryStreamingService( avs::uid clid)
				: GeometryStreamingService(clid)
			{
				this->geometryStore = &GeometryStore::GetInstance();
			}
			virtual ~PluginGeometryStreamingService() = default;

			static ClientStoppedRenderingNodeFn callback_clientStoppedRenderingNode;
			static ClientStartedRenderingNodeFn callback_clientStartedRenderingNode;
		private:
			virtual bool clientStoppedRenderingNode_Internal(avs::uid clientID, avs::uid nodeID)
			{
				if (callback_clientStoppedRenderingNode)
				{
					if (!callback_clientStoppedRenderingNode(clientID, nodeID))
					{
						// This is ok, it means we probably already deleted the node.
							//TELEPORT_CERR << "callback_clientStoppedRenderingNode failed for node " << nodeID << "(" << geometryStore->getNodeName(nodeID) << ")" << "\n";
						return false;
					}
					return true;
				}
				return false;
			}
			virtual bool clientStartedRenderingNode_Internal(avs::uid clientID, avs::uid nodeID)
			{
				if (callback_clientStartedRenderingNode)
				{
					if (!callback_clientStartedRenderingNode(clientID, nodeID))
					{
						//	TELEPORT_CERR << "callback_clientStartedRenderingNode failed for node " << nodeID << "(" << geometryStore->getNodeName(nodeID) << ")" << "\n";
						return false;
					}
					return true;
				}
				return false;
			}
		};
	}
}