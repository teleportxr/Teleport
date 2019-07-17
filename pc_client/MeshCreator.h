#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include "Simul/Platform/Crossplatform/Mesh.h"


/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class MeshCreator final : public avs::GeometryTargetBackendInterface
{
public:
	MeshCreator();
	~MeshCreator();


	// Inherited via GeometryTargetBackendInterface
	void ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::vec3 * vertices) override;
	void ensureNormals(unsigned long long shape_uid, int startNormal, int normalCount, const avs::vec3* normals) override;
	void ensureTangents(unsigned long long shape_uid, int startTangent, int tangentCount, const avs::vec4* tangents) override;
	void ensureTexCoord0(unsigned long long shape_uid, int startTexCoord0, int texCoordCount0, const avs::vec2* texCoords0) override;
	void ensureTexCoord1(unsigned long long shape_uid, int startTexCoord1, int texCoordCount1, const avs::vec2* texCoords1) override;
	void ensureColors(unsigned long long shape_uid, int startColor, int colorCount, const avs::vec4* colors) override;
	void ensureJoints(unsigned long long shape_uid, int startJoint, int jointCount, const avs::vec4* joints) override;
	void ensureWeights(unsigned long long shape_uid, int startWeight, int weightCount, const avs::vec4* weights) override;
	void ensureFaces(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::int3 * faces) override;
	void ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, const unsigned int* indices) override;

	void Create(simul::crossplatform::RenderPlatform* renderPlatform);

	void BeginDraw(simul::crossplatform::DeviceContext& deviceContext, simul::crossplatform::ShadingMode shadingMode = simul::crossplatform::ShadingMode::SHADING_MODE_SHADED);
	void Draw(simul::crossplatform::DeviceContext& deviceContext, int meshIndex = 0);
	void EndDraw(simul::crossplatform::DeviceContext& deviceContext);

	void SetMaterialForSubMesh(int meshIndex, simul::crossplatform::Material* material);

public:
	simul::crossplatform::ShadingMode m_ShadingMode;

private:
	avs::uid shape_uid = -1;
	std::unique_ptr<simul::crossplatform::Mesh> m_Mesh;
	
	int					m_VertexCount	= 0;
	int					m_IndexCount	= 0;
	int					m_PolygonCount	= 0;

	std::unique_ptr<float[]>			m_Vertices		= nullptr;
	std::unique_ptr<float[]>			m_Normals		= nullptr;
	std::unique_ptr<float[]>			m_Tangents		= nullptr;
	std::unique_ptr<float[]>			m_UV0s			= nullptr;
	std::unique_ptr<float[]>			m_UV1s			= nullptr;
	std::unique_ptr<float[]>			m_Colors		= nullptr;
	std::unique_ptr<float[]>			m_Joints		= nullptr;
	std::unique_ptr<float[]>			m_Weights		= nullptr;
	std::unique_ptr<unsigned int[]>		m_Indices		= nullptr;

	inline bool SetAndCheckShapeUID(const avs::uid& uid)
	{
		if (shape_uid == -1)
		{
			shape_uid = uid;
			return true;
		}
		else if (shape_uid == uid)
		{
			return true;
		}
		else
			return false;
	}

#define CHECK_SHAPE_UID(x) if (!SetAndCheckShapeUID(x)) { SIMUL_BREAK_ONCE("Invalid shape_uid.\n"); return; }

	static unsigned int s_TotalSubMeshes;
};

