// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include <deque>

#include "GlGeometry.h"
#include "App.h"

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class MeshCreator: public avs::GeometryTargetBackendInterface
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

    void Create();

private:
    struct GlMesh
    {
        avs::uid shape_uid;             //-1 means pre-initialised.
        OVR::GlGeometry geometry;
        OVR::VertexAttribs vertex_attribs;
        OVR::Array<OVR::TriangleIndex> indices;
    }m_Mesh;

    inline bool SetAndCheckShapeUID(const avs::uid& uid)
    {
        if(m_Mesh.shape_uid == (avs::uid)-1)
        {
            m_Mesh.shape_uid = uid;
            return true;
        }
        else if(m_Mesh.shape_uid != uid)
        {
            return false;
        }
        else
            return false;

    }
};

