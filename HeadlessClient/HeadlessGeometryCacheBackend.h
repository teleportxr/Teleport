#pragma once

#include "ConnectionReport.h"
#include "TeleportClient/GeometryCacheBackendInterface.h"
#include <libavstream/node.h>
#include <libavstream/geometry/mesh_interface.hpp>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace teleport
{
	namespace core
	{
		struct ApplyAnimation;
	}
}

//! Records what the server has streamed to us, without creating any GPU resources.
//!
//! Geometry payloads arrive on the pipeline's processing thread while SessionClient drains the
//! acknowledgement lists from the tick thread, so every accessor here is mutex-guarded.
//!
//! Resources supplied as pointers (MeshPointer/TexturePointer/MaterialPointer) are recorded by
//! uid and URL only - we deliberately never fetch the asset. The server considers its job done
//! once the pointer has been delivered, so acknowledging without downloading is correct.
class HeadlessGeometryCacheBackend : public teleport::client::GeometryCacheBackendInterface
{
public:
	//! A node as streamed by the server, retaining only the protocol-level fields.
	struct TrackedNode
	{
		std::string			 name;
		avs::NodeDataType	 dataType = avs::NodeDataType::None;
		avs::uid			 dataUid  = 0;
		avs::uid			 parentUid = 0;
		avs::uid			 skeletonUid = 0;
		std::vector<avs::uid> materials;
		std::vector<avs::uid> animations;
		std::string			 url;			//!< Set for Link nodes.
	};

	//! A resource the server gave us a URL for rather than inline data.
	struct TrackedPointer
	{
		avs::GeometryPayloadType type = avs::GeometryPayloadType::Invalid;
		std::string				 url;
	};

	HeadlessGeometryCacheBackend() = default;
	virtual ~HeadlessGeometryCacheBackend() = default;

	// teleport::client::GeometryCacheBackendInterface - drained by SessionClient each frame.
	const std::vector<avs::uid> &GetCompletedNodes() const override;
	std::vector<avs::uid> GetReceivedResources() const override;
	std::vector<avs::uid> GetResourceRequests() const override;
	void ClearCompletedNodes() override;
	void ClearReceivedResources() override;
	void ClearResourceRequests() override;
	void ClearAll() override;

	// Recording, called from the geometry pipeline thread.

	//! Acknowledge a resource of any payload type. Safe to call for payloads we do not parse.
	void ReceivedResource(avs::uid uid);
	//! Note a uid a node referred to that we have not been sent, so `geometry` can report it.
	void NoteReferencedResource(avs::uid uid);
	void TrackNode(avs::uid uid, const avs::Node &node);
	void UntrackNode(avs::uid uid);
	void TrackPointer(avs::uid uid, avs::GeometryPayloadType type, const std::string &url);
	void TrackSkeleton(avs::uid uid, const avs::Skeleton &skeleton);
	//! Record the animation state the server last applied to a node, keyed by node and layer, so
	//! `geometry nodes` can report what should be playing. Called from the session command thread.
	void TrackAnimationState(const teleport::core::ApplyAnimation &applyAnimation);
	//! Record a payload type we acknowledged but did not parse, for the `geometry` report.
	void CountUnparsedPayload(avs::GeometryPayloadType type);

	//! Everything the `geometry` command can report, gathered under one lock. Rendering
	//! - prose or JSON - happens in the caller; see ConnectionReport.h.
	GeometryReport GetReport() const;

private:
	mutable std::mutex mutex;

	//! Pending acknowledgements. Cleared by SessionClient once sent.
	std::vector<avs::uid> receivedResources;
	std::vector<avs::uid> completedNodes;
	std::vector<avs::uid> resourceRequests;

	//! Cumulative record, never cleared by the ack drain - this is what `geometry` reports.
	std::map<avs::uid, TrackedNode>		 nodes;
	//! Last animation state per node, per layer. Kept apart from TrackedNode because an
	//! ApplyAnimation can arrive before the node it names has been streamed.
	std::map<avs::uid, std::map<int32_t, GeometryNodeAnimationState>> nodeAnimationStates;
	std::map<avs::uid, TrackedPointer>	 pointers;
	std::map<avs::uid, std::string>		 skeletons;
	std::set<avs::uid>					 allReceivedResources;
	std::set<avs::uid>					 referencedButNotReceived;
	std::map<avs::GeometryPayloadType, size_t> unparsedPayloadCounts;
	size_t								 nodesRemoved = 0;

	//! GetCompletedNodes() hands back a reference, which would otherwise escape the mutex.
	//! It and ClearCompletedNodes() are only ever called from the tick thread, so we copy under
	//! the lock into this buffer and return a reference to that instead.
	mutable std::vector<avs::uid> completedNodesSnapshot;
};
