#include "GeometryCache.h"

using namespace clientrender;

GeometryCache::GeometryCache(clientrender::NodeManager *nm)
		: mNodeManager(nm),
		  mIndexBufferManager(&clientrender::IndexBuffer::Destroy),
		  mTextureManager(&clientrender::Texture::Destroy),
		  mUniformBufferManager(&clientrender::UniformBuffer::Destroy),
		  mVertexBufferManager(&clientrender::VertexBuffer::Destroy)
{
}

GeometryCache::~GeometryCache()
{

}
