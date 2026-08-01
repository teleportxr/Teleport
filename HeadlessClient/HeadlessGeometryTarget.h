#pragma once

#include <libavstream/geometry/mesh_interface.hpp>
#include <string>

class HeadlessGeometryCacheBackend;

//! Receives decoded geometry and records it in the cache instead of creating GPU resources.
//!
//! This is the avs::GeometryTargetBackendInterface that ClientRender fills with
//! clientrender::ResourceCreator. Everything here is bookkeeping only - no meshes, textures or
//! materials are ever built, so the headless client needs no graphics API.
class HeadlessGeometryTarget final : public avs::GeometryTargetBackendInterface
{
public:
	explicit HeadlessGeometryTarget(HeadlessGeometryCacheBackend *cache);
	virtual ~HeadlessGeometryTarget();

	// avs::GeometryTargetBackendInterface
	avs::Result CreateMesh(avs::MeshCreate &meshCreate) override;
	void		CreateTexture(avs::uid server_uid, avs::uid id, const avs::Texture &texture) override;
	void		CreateMaterial(avs::uid server_uid, avs::uid id, const avs::Material &material) override;
	void		CreateNode(avs::uid server_uid, avs::uid id, const avs::Node &node) override;
	void		CreateSkeleton(avs::uid server_uid, avs::uid id, const avs::Skeleton &skeleton) override;
	avs::Result CreateAnimation(avs::uid server_uid, avs::uid id, teleport::core::Animation &animation, avs::AxesStandard sourceAxesStandard) override;
	void		DeleteNode(avs::uid server_uid, avs::uid id) override;

	size_t GetMeshesCreated() const { return meshesCreated; }
	size_t GetTexturesCreated() const { return texturesCreated; }
	size_t GetMaterialsCreated() const { return materialsCreated; }
	size_t GetNodesCreated() const { return nodesCreated; }

private:
	HeadlessGeometryCacheBackend *cache = nullptr;

	size_t meshesCreated	= 0;
	size_t texturesCreated	= 0;
	size_t materialsCreated = 0;
	size_t nodesCreated		= 0;
};
