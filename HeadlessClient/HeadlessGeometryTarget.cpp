#include "HeadlessGeometryTarget.h"
#include "HeadlessGeometryCacheBackend.h"
#include "TeleportCore/Logging.h"

// Division of labour with HeadlessGeometryDecoder: the decoder acknowledges every payload by uid,
// because it alone sees payload types that have no method on this interface. This class records
// structure only, so nothing here acknowledges anything and no uid is counted twice.

HeadlessGeometryTarget::HeadlessGeometryTarget(HeadlessGeometryCacheBackend *c)
	: cache(c)
{
}

HeadlessGeometryTarget::~HeadlessGeometryTarget()
{
}

avs::Result HeadlessGeometryTarget::CreateMesh(avs::MeshCreate &meshCreate)
{
	meshesCreated++;
	TELEPORT_LOG("Geometry: mesh {} \"{}\" ({} elements, not built)", meshCreate.mesh_uid, meshCreate.name, meshCreate.m_MeshElementCreate.size());
	return avs::Result::OK;
}

void HeadlessGeometryTarget::CreateTexture(avs::uid server_uid, avs::uid id, const avs::Texture &texture)
{
	texturesCreated++;
	TELEPORT_LOG("Geometry: texture {} \"{}\" (not built)", id, texture.name);
}

void HeadlessGeometryTarget::CreateMaterial(avs::uid server_uid, avs::uid id, const avs::Material &material)
{
	materialsCreated++;
	TELEPORT_LOG("Geometry: material {} \"{}\" (not built)", id, material.name);
}

void HeadlessGeometryTarget::CreateNode(avs::uid server_uid, avs::uid id, const avs::Node &node)
{
	nodesCreated++;
	if (cache)
		cache->TrackNode(id, node);
	TELEPORT_LOG("Geometry: node {} \"{}\" type {} data {}", id, node.name, (int)node.data_type, node.data_uid);
}

void HeadlessGeometryTarget::CreateSkeleton(avs::uid server_uid, avs::uid id, const avs::Skeleton &skeleton)
{
	if (cache)
		cache->TrackSkeleton(id, skeleton);
	TELEPORT_LOG("Geometry: skeleton {} \"{}\" ({} bones)", id, skeleton.name, skeleton.boneIDs.size());
}

avs::Result HeadlessGeometryTarget::CreateAnimation(avs::uid server_uid, avs::uid id, teleport::core::Animation &animation, avs::AxesStandard sourceAxesStandard)
{
	TELEPORT_LOG("Geometry: animation {} (not built)", id);
	return avs::Result::OK;
}

void HeadlessGeometryTarget::DeleteNode(avs::uid server_uid, avs::uid id)
{
	if (cache)
		cache->UntrackNode(id);
	TELEPORT_LOG("Geometry: node {} removed", id);
}
