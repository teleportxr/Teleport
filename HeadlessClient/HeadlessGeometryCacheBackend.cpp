#include "HeadlessGeometryCacheBackend.h"
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

void HeadlessGeometryCacheBackend::CountUnparsedPayload(avs::GeometryPayloadType type)
{
	std::lock_guard<std::mutex> lock(mutex);
	unparsedPayloadCounts[type]++;
}

std::string HeadlessGeometryCacheBackend::GetSummary() const
{
	std::lock_guard<std::mutex> lock(mutex);
	std::ostringstream o;
	o << "Nodes tracked:        " << nodes.size() << " (" << nodesRemoved << " removed)\n";
	o << "Skeletons:            " << skeletons.size() << "\n";
	o << "Resources received:   " << allReceivedResources.size() << "\n";
	o << "Pointer resources:    " << pointers.size() << " (URLs recorded, not downloaded)\n";
	o << "Referenced, unsent:   " << referencedButNotReceived.size() << "\n";
	o << "Acks pending send:    " << receivedResources.size() << " resources, " << completedNodes.size() << " nodes\n";
	if (!unparsedPayloadCounts.empty())
	{
		o << "Acknowledged without parsing:\n";
		for (const auto &u : unparsedPayloadCounts)
			o << "  " << avs::stringOf(u.first) << ": " << u.second << "\n";
	}
	return o.str();
}

std::string HeadlessGeometryCacheBackend::GetNodeReport() const
{
	std::lock_guard<std::mutex> lock(mutex);
	if (nodes.empty())
		return "No nodes tracked.\n";
	std::ostringstream o;
	for (const auto &n : nodes)
	{
		o << n.first << " \"" << n.second.name << "\" type=" << (int)n.second.dataType;
		if (n.second.dataUid)
			o << " data=" << n.second.dataUid;
		if (n.second.parentUid)
			o << " parent=" << n.second.parentUid;
		if (n.second.skeletonUid)
			o << " skeleton=" << n.second.skeletonUid;
		if (!n.second.materials.empty())
			o << " materials=" << n.second.materials.size();
		if (!n.second.animations.empty())
			o << " animations=" << n.second.animations.size();
		if (!n.second.url.empty())
			o << " url=" << n.second.url;
		o << "\n";
	}
	return o.str();
}

std::string HeadlessGeometryCacheBackend::GetResourceReport() const
{
	std::lock_guard<std::mutex> lock(mutex);
	std::ostringstream o;
	if (pointers.empty())
	{
		o << "No pointer resources.\n";
	}
	else
	{
		o << "Pointer resources (not downloaded):\n";
		for (const auto &p : pointers)
			o << "  " << p.first << " " << avs::stringOf(p.second.type) << " " << p.second.url << "\n";
	}
	if (!referencedButNotReceived.empty())
	{
		o << "Referenced but never sent:\n  ";
		for (avs::uid u : referencedButNotReceived)
			o << u << " ";
		o << "\n";
	}
	return o.str();
}
