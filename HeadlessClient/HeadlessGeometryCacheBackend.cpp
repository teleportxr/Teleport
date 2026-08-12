#include "HeadlessGeometryCacheBackend.h"
#include "TeleportCore/CommonNetworking.h"
#include "TeleportCore/Logging.h"
#include <libavstream/common.hpp>	// avs::stringOf(GeometryPayloadType)
#include <algorithm>
#include <sstream>

const std::vector<avs::uid> &HeadlessGeometryCacheBackend::GetCompletedNodes() const
{
	std::lock_guard<std::mutex> lock(mutex);
	completedNodesSnapshot = completedNodes;
	return completedNodesSnapshot;
}

std::vector<avs::uid> HeadlessGeometryCacheBackend::GetReceivedResources() const
{
	std::lock_guard<std::mutex> lock(mutex);
	std::vector<avs::uid> r = receivedResources;
	std::sort(r.begin(), r.end());
	r.erase(std::unique(r.begin(), r.end()), r.end());
	return r;
}

std::vector<avs::uid> HeadlessGeometryCacheBackend::GetResourceRequests() const
{
	std::lock_guard<std::mutex> lock(mutex);
	return resourceRequests;
}

void HeadlessGeometryCacheBackend::ClearCompletedNodes()
{
	std::lock_guard<std::mutex> lock(mutex);
	completedNodes.clear();
}

void HeadlessGeometryCacheBackend::ClearReceivedResources()
{
	std::lock_guard<std::mutex> lock(mutex);
	receivedResources.clear();
}

void HeadlessGeometryCacheBackend::ClearResourceRequests()
{
	std::lock_guard<std::mutex> lock(mutex);
	resourceRequests.clear();
}

void HeadlessGeometryCacheBackend::ClearAll()
{
	std::lock_guard<std::mutex> lock(mutex);
	receivedResources.clear();
	completedNodes.clear();
	resourceRequests.clear();
	nodes.clear();
	pointers.clear();
	skeletons.clear();
	allReceivedResources.clear();
	referencedButNotReceived.clear();
	unparsedPayloadCounts.clear();
	nodesRemoved = 0;
}

void HeadlessGeometryCacheBackend::ReceivedResource(avs::uid uid)
{
	if (!uid)
		return;
	std::lock_guard<std::mutex> lock(mutex);
	receivedResources.push_back(uid);
	allReceivedResources.insert(uid);
	referencedButNotReceived.erase(uid);
}

void HeadlessGeometryCacheBackend::NoteReferencedResource(avs::uid uid)
{
	if (!uid)
		return;
	std::lock_guard<std::mutex> lock(mutex);
	if (allReceivedResources.find(uid) == allReceivedResources.end())
		referencedButNotReceived.insert(uid);
}

void HeadlessGeometryCacheBackend::TrackNode(avs::uid uid, const avs::Node &node)
{
	if (!uid)
		return;
	{
		std::lock_guard<std::mutex> lock(mutex);
		TrackedNode &t = nodes[uid];
		t.name		   = node.name;
		t.dataType	   = node.data_type;
		t.dataUid	   = node.data_uid;
		t.parentUid	   = node.parentID;
		t.skeletonUid  = node.skeletonID;
		t.materials	   = node.materials;
		t.animations   = node.animations;
		t.url		   = node.url;
		// A node is "complete" for a headless client as soon as it is parsed: we hold no meshes
		// or textures to wait on, and the server only needs to know the node reached us.
		completedNodes.push_back(uid);
	}
	// Record the uids this node depends on so `geometry` can show what we were told about but
	// never received. Done outside the lock above via the public helper, which takes it again.
	NoteReferencedResource(node.data_uid);
	NoteReferencedResource(node.skeletonID);
	for (avs::uid m : node.materials)
		NoteReferencedResource(m);
	for (avs::uid a : node.animations)
		NoteReferencedResource(a);
}

void HeadlessGeometryCacheBackend::UntrackNode(avs::uid uid)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (nodes.erase(uid))
		nodesRemoved++;
	// A removed node plays nothing, and a later node reusing the uid must not inherit this.
	nodeAnimationStates.erase(uid);
	// Drop any pending completion for a node the server has since removed.
	completedNodes.erase(std::remove(completedNodes.begin(), completedNodes.end(), uid), completedNodes.end());
}

void HeadlessGeometryCacheBackend::TrackPointer(avs::uid uid, avs::GeometryPayloadType type, const std::string &url)
{
	if (!uid)
		return;
	std::lock_guard<std::mutex> lock(mutex);
	TrackedPointer &p = pointers[uid];
	p.type			  = type;
	p.url			  = url;
}

void HeadlessGeometryCacheBackend::TrackSkeleton(avs::uid uid, const avs::Skeleton &skeleton)
{
	if (!uid)
		return;
	std::lock_guard<std::mutex> lock(mutex);
	skeletons[uid] = skeleton.name;
}

void HeadlessGeometryCacheBackend::TrackAnimationState(const teleport::core::ApplyAnimation &applyAnimation)
{
	if (!applyAnimation.nodeID)
		return;
	std::lock_guard<std::mutex> lock(mutex);
	// One state per layer, the later command replacing the earlier: layers blend on a real client,
	// they do not queue, so only the most recent state per layer is in force.
	GeometryNodeAnimationState &state = nodeAnimationStates[applyAnimation.nodeID][applyAnimation.animLayer];
	state.animation					  = applyAnimation.animationID;
	state.layer						  = applyAnimation.animLayer;
	state.timeAtTimestamp			  = applyAnimation.animTimeAtTimestamp;
	state.speed						  = applyAnimation.speedUnitsPerSecond;
	state.loop						  = applyAnimation.loop;
	state.timestampUs				  = applyAnimation.timestampUs;
}

void HeadlessGeometryCacheBackend::CountUnparsedPayload(avs::GeometryPayloadType type)
{
	std::lock_guard<std::mutex> lock(mutex);
	unparsedPayloadCounts[type]++;
}

GeometryReport HeadlessGeometryCacheBackend::GetReport() const
{
	std::lock_guard<std::mutex> lock(mutex);
	GeometryReport				report;
	report.hasCache						= true;
	report.counts.nodes					= nodes.size();
	report.counts.nodesRemoved			= nodesRemoved;
	report.counts.skeletons				= skeletons.size();
	report.counts.resourcesReceived		= allReceivedResources.size();
	report.counts.pointers				= pointers.size();
	report.counts.referencedUnsent		= referencedButNotReceived.size();
	report.counts.pendingResourceAcks	= receivedResources.size();
	report.counts.pendingNodeAcks		= completedNodes.size();

	for (const auto &u : unparsedPayloadCounts)
		report.unparsed.push_back({avs::stringOf(u.first), u.second});

	report.nodes.reserve(nodes.size());
	GeometryReport				report;
	report.hasCache						= true;
	report.counts.nodes					= nodes.size();
	report.counts.nodesRemoved			= nodesRemoved;
	report.counts.skeletons				= skeletons.size();
	report.counts.resourcesReceived		= allReceivedResources.size();
	report.counts.pointers				= pointers.size();
	report.counts.referencedUnsent		= referencedButNotReceived.size();
	report.counts.pendingResourceAcks	= receivedResources.size();
	report.counts.pendingNodeAcks		= completedNodes.size();

	for (const auto &u : unparsedPayloadCounts)
		report.unparsed.push_back({avs::stringOf(u.first), u.second});

	report.nodes.reserve(nodes.size());
	for (const auto &n : nodes)
	{
		GeometryNodeEntry entry;
		entry.uid		 = n.first;
		entry.name		 = n.second.name;
		entry.dataType	 = (int)n.second.dataType;
		entry.data		 = n.second.dataUid;
		entry.parent	 = n.second.parentUid;
		entry.skeleton	 = n.second.skeletonUid;
		entry.materials	 = n.second.materials.size();
		entry.animations = n.second.animations.size();
		auto a			 = nodeAnimationStates.find(n.first);
		if (a != nodeAnimationStates.end())
		{
			entry.animationStates.reserve(a->second.size());
			for (const auto &s : a->second)
				entry.animationStates.push_back(s.second);
		}
		entry.url = n.second.url;
		report.nodes.push_back(std::move(entry));
	}

	report.pointers.reserve(pointers.size());
	for (const auto &p : pointers)
		report.pointers.push_back({p.first, avs::stringOf(p.second.type), p.second.url});

	report.referencedUnsent.assign(referencedButNotReceived.begin(), referencedButNotReceived.end());
	return report;
}
