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

std::vector<avs::uid> GeometryCache::GetResourceRequests() const
{
	std::vector<avs::uid> resourceRequests = m_ResourceRequests;
	//Remove duplicates.
	std::sort(resourceRequests.begin(), resourceRequests.end());
	resourceRequests.erase(std::unique(resourceRequests.begin(), resourceRequests.end()), resourceRequests.end());

	return resourceRequests;
}
void GeometryCache::ClearResourceRequests()
{
	m_ResourceRequests.clear();
}

std::vector<avs::uid> GeometryCache::GetReceivedResources() const
{
	return m_ReceivedResources;
}

void GeometryCache::ClearReceivedResources()
{
	m_ReceivedResources.clear();
}

std::vector<avs::uid> GeometryCache::GetCompletedNodes() const
{
	return m_CompletedNodes;
}

void GeometryCache::ClearCompletedNodes()
{
	m_CompletedNodes.clear();
}