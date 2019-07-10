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
    void ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::vec3* vertices) override;
    void ensureFaces(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::int3* faces) override;
    void ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, unsigned int* indices) override;

    void Create(GLenum usage = GL_DYNAMIC_DRAW);

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
        if(m_Mesh.shape_uid == -1)
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

