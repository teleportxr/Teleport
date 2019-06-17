#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>


/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class MeshCreator: public avs::GeometryTargetBackendInterface
{
public:
	MeshCreator();
	~MeshCreator();


	// Inherited via GeometryTargetBackendInterface
	void ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::vec3 * vertices) override;

	void ensureFaces(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::int3 * faces) override;

};

