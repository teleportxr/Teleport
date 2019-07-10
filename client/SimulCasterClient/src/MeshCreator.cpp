// (C) Copyright 2018-2019 Simul Software Ltd
#include "MeshCreator.h"

using namespace avs;
using namespace OVR;

MeshCreator::MeshCreator()
{
    m_Mesh.shape_uid = -1;
}

MeshCreator::~MeshCreator()
{
    m_Mesh.shape_uid = -1;
    m_Mesh.geometry.Free();
}
void MeshCreator::ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const vec3* vertices)
{
    if(!SetAndCheckShapeUID(shape_uid))
    {
        OVR_WARN("shape_uid is already set or is incorrect.");
        return;
    }

    m_Mesh.vertex_attribs.position.Resize(static_cast<size_t>(vertexCount));
    memcpy(m_Mesh.indices.GetDataPtr(), vertices, static_cast<size_t>(vertexCount));
}

void MeshCreator::ensureFaces(unsigned long long shape_uid, int startVertex, int vertexCount, const int3 * faces)
{
    if(!SetAndCheckShapeUID(shape_uid))
    {
        OVR_WARN("shape_uid is already set or is incorrect.");
        return;
    }
}
void MeshCreator::ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, unsigned int* indices)
{
    if(!SetAndCheckShapeUID(shape_uid))
    {
        OVR_WARN("shape_uid is already set or is incorrect.");
        return;
    }

    m_Mesh.indices.Resize(static_cast<size_t>(indexCount));
    memcpy(m_Mesh.indices.GetDataPtr(), indices, static_cast<size_t>(indexCount));
}

void MeshCreator::Create(GLenum usage)
{
    if(m_Mesh.indices.IsEmpty() || m_Mesh.vertex_attribs.position.IsEmpty())
        return;

    m_Mesh.geometry.Create(m_Mesh.vertex_attribs, m_Mesh.indices, usage);
}

