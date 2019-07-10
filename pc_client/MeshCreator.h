#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include "Simul/Platform/Crossplatform/Mesh.h"


/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class MeshCreator final : public avs::GeometryTargetBackendInterface
{
public:
	MeshCreator(simul::crossplatform::RenderPlatform* renderPlatfrom);
	~MeshCreator();


	// Inherited via GeometryTargetBackendInterface
	void ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::vec3 * vertices) override;
	void ensureFaces(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::int3 * faces) override;
	void ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, const unsigned int* indices) override;

	void Create();

	void BeginDraw(simul::crossplatform::DeviceContext& deviceContext, simul::crossplatform::ShadingMode shadingMode = simul::crossplatform::ShadingMode::SHADING_MODE_SHADED);
	void Draw(simul::crossplatform::DeviceContext& deviceContext, int meshIndex = 0);
	void EndDraw(simul::crossplatform::DeviceContext& deviceContext);

	void SetMaterialForSubMesh(int meshIndex, simul::crossplatform::Material* material);

public:
	simul::crossplatform::ShadingMode m_ShadingMode;

private:
	simul::crossplatform::RenderPlatform* m_RP;

	avs::uid shape_uid;
	std::unique_ptr<simul::crossplatform::Mesh> m_Mesh;
	
	//Provide the Submesh with a default material upon creation.
	simul::crossplatform::Material* m_Material;
	
	unsigned int numVertices = 0;
	unsigned int numIndices = 0;

	int					lPolygonVertexCount		= 0;
	const float*		lVertices				= nullptr;
	const float*		lNormals				= nullptr;
	const float*		lUVs					= nullptr;
	int					lPolygonCount			= 0;
	const unsigned int* lIndices				= nullptr;
	//const unsigned short* sIndices;

	inline bool SetAndCheckShapeUID(const avs::uid& uid)
	{
		if (shape_uid == -1)
		{
			shape_uid = uid;
			return true;
		}
		else if (shape_uid!= uid)
		{
			return false;
		}
		else
			return false;

	}

	static unsigned int s_TotalSubMeshes;
};

