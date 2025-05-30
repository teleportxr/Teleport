#pragma once
#include <set>
#include "libavstream/geometry/mesh_interface.hpp"
#include "libavstream/pipeline.hpp"
#include "TeleportCore/CommonNetworking.h"
#include "ClientNetworkContext.h"
#include "GeometryEncoder.h"
#include "GeometryStore.h"
#include "TeleportServer/Export.h"
#include "TeleportServer/Exports.h"

#if TELEPORT_INTERNAL_CHECKS
#define TELEPORT_DEBUG_RESOURCE_STREAMING 1
#else
#define TELEPORT_DEBUG_RESOURCE_STREAMING 1
#endif
 
namespace teleport
{
	namespace server
	{
		class ClientMessaging;
		//! A tracked resource. This keeps track of whether a resource has been sent to the client, when we sent it, and if we have an acknowledgement of the client receiving it.
		//! The value clientNeeds may be false, if a resource was needed but is now not.
		struct TrackedResource
		{
			bool clientNeeds=true;			// whether we THINK the client NEEDS the resource.
			bool sent=false;				// Whether we have actually sent the resource,
			int64_t sent_server_time_us=0;	// and when we sent it.
			bool acknowledged=false;		// Whether the client acknowledged receiving the resource.
			bool confirmedRemoved=false;	// Whether the client has confirmed that it had it and removed it.
		};
		//! This per-client class tracks the resources and nodes that the client needs,
		//! and returns them via GeometryRequesterBackendInterface to the encoder.
		class TELEPORT_SERVER_API GeometryStreamingService : public avs::GeometryRequesterBackendInterface
		{
			ClientMessaging &clientMessaging;
		public:
			GeometryStreamingService(ClientMessaging &clientMessaging,avs::uid clid);
			virtual ~GeometryStreamingService();

			virtual bool hasResource(avs::uid resourceID) const override;

			//! Tell the StreamingService that a resource has been sent.
			virtual void encodedResource(avs::uid resourceID) override;
			//! Tell the StreamingService that a resource has been received by the client.
			virtual void confirmResource(avs::uid resourceID) override;
			
			void confirmResourceRemoved(avs::uid resource_uid);
			
			void updateResourcesToStream(int32_t minimumPriority);
			//! The Geometry Streaming Service tracks 
			void getResourcesToStream(std::set<avs::uid>& outNodeIDs
				,std::set<avs::uid>& removeNodeIDs
				,std::set<avs::uid>& genericTextureUids
				,std::set<avs::uid>& meshes
				,std::set<avs::uid>& materials
				,std::set<avs::uid>& textures
				,std::set<avs::uid>& skeletons
				,std::set<avs::uid>& bones
				,std::set<avs::uid>& animations
				,std::set<avs::uid>& textCanvases
				,std::set<avs::uid>& fontAtlases) const;
				
			void getNodesToUpdateMovement(std::set<avs::uid>& nodes_to_update_movement,int64_t timestamp);
			virtual avs::AxesStandard getClientAxesStandard() const override;
			virtual avs::RenderingFeatures getClientRenderingFeatures() const override;

			virtual void startStreaming(ClientNetworkContext* context, const teleport::core::Handshake& handshake);
			//Stop streaming to client.
			void stopStreaming();

			virtual void tick(float deltaTime);

			virtual void reset();
			
			const std::set<avs::uid>& getNodesToStream();
			const std::set<avs::uid>& getStreamedNodeIDs();
			// The origin node for the client - must have this in order to stream geometry.
			void setOriginNode(avs::uid nodeID);
			/// Returns true if the node is in the GeometryStore and thus could be added.
			bool streamNode(avs::uid nodeID);
			bool unstreamNode(avs::uid nodeID);
			bool isStreamingNode(avs::uid nodeID);
			void addGenericTexture(avs::uid id);
			void removeGenericTexture(avs::uid id);
			bool startedRenderingNode(avs::uid nodeID);
			bool stoppedRenderingNode(avs::uid nodeID);
			//! From the list of uid's given, which do we not think the client has?
			size_t GetNewUIDs(std::vector<avs::uid>& outUIDs);
			void updateNodePriority(avs::uid nodeID);
		protected:
			void recalculateUnconfirmedPriorityCounts();
			int32_t getPriorityForNode(avs::uid nodeID) const;
			int64_t movement_update_timestamp=0;
			std::set<avs::uid> streamed_node_uids;
			avs::uid originNodeId = 0;

			// The lowest priority for which the client has confirmed all the nodes we sent.
			// We only send lower-priority nodes when all higher priorities have been confirmed.
			int32_t lowest_confirmed_node_priority=-100000;
			// How many nodes we have unconfirmed 
			std::map<int32_t,uint32_t> unconfirmed_priority_counts;
			GeometryStore* geometryStore = nullptr;

			teleport::core::Handshake handshake;

			ClientNetworkContext* clientNetworkContext = nullptr;
			GeometryEncoder geometryEncoder;

			// The following MIGHT be moved later to a separate Pipeline class:
			std::unique_ptr<avs::Pipeline> avsPipeline;
			std::unique_ptr<avs::GeometrySource> avsGeometrySource;
			std::unique_ptr<avs::GeometryEncoder> avsGeometryEncoder;

			std::map<avs::uid,std::shared_ptr<TrackedResource>> trackedResources;
			const TrackedResource &GetTrackedResource(avs::uid u) const;
			TrackedResource &GetTrackedResource(avs::uid u);
			//! Here is the set of which nodes the client *should* have, ignoring priority:
			std::set<avs::uid> nodesToStream;

			//! Here is the set of nodes actually to stream. When higher priority nodes are acknowledged, lower priority nodes AND their resources are added.
			std::map<avs::uid,int> streamedNodes;
			std::set<avs::uid> streamedGenericTextures; // Textures that are not specifically specified in a material, e.g. lightmaps.

			// Node resources are refcounted, they could be requested by more than one node, and only when no node references them should they be removed.
			std::map<avs::uid,int> streamedMeshes;		
			std::map<avs::uid,int> streamedMaterials;
			std::map<avs::uid,int> streamedTextures;
			std::map<avs::uid,int> streamedSkeletons;
			std::map<avs::uid,int> streamedBones;
			std::map<avs::uid,int> streamedAnimations;
			std::map<avs::uid,int> streamedTextCanvases;
			std::map<avs::uid,int> streamedFontAtlases;
			std::set<avs::uid> nodesToRemove;

			//Recursively obtains the resources from the mesh node, and its child nodes.
			void GetMeshNodeResources(avs::uid nodeID, const avs::Node& node, std::vector<avs::MeshNodeResources>& outMeshResources) const;
			void GetSkeletonNodeResources(avs::uid nodeID, const avs::Node &node, std::vector<avs::MeshNodeResources> &outSkeletonNodeResources) const;
			avs::uid clientId = 0;
		public:
			static ClientStoppedRenderingNodeFn callback_clientStoppedRenderingNode;
			static ClientStartedRenderingNodeFn callback_clientStartedRenderingNode;
		private:
			void AddNodeAndItsResourcesToStreamed(avs::uid u,bool remove=false);
			void RemoveNodeAndItsResourcesFromStreamed(avs::uid u);
		};
	}
}