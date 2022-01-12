#include "GeometryCache.h"

using namespace scr;

GeometryCache::GeometryCache(scr::NodeManager *nm)
		: mNodeManager(nm),
		  mIndexBufferManager(&scr::IndexBuffer::Destroy),
		  mTextureManager(&scr::Texture::Destroy),
		  mUniformBufferManager(&scr::UniformBuffer::Destroy),
		  mVertexBufferManager(&scr::VertexBuffer::Destroy)
{
}

GeometryCache::~GeometryCache()
{

}
