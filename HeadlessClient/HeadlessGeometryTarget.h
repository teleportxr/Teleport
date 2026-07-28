#pragma once

#include <memory>

class HeadlessGeometryTarget
{
public:
	HeadlessGeometryTarget();
	~HeadlessGeometryTarget();

	void LogGeometryEvent(const std::string &event);

	size_t GetMeshesCreated() const { return meshesCreated; }
	size_t GetTexturesCreated() const { return texturesCreated; }
	size_t GetMaterialsCreated() const { return materialsCreated; }
	size_t GetNodesCreated() const { return nodesCreated; }

private:
	size_t meshesCreated = 0;
	size_t texturesCreated = 0;
	size_t materialsCreated = 0;
	size_t nodesCreated = 0;
};
