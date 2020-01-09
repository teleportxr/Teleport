#include "GeometryEncoder.h"

#include <algorithm>
#include <set>

#include "libavstream/common.hpp"

#include "CasterSettings.h"

using namespace SCServer;

//Clear a passed vector of UIDs that are believed to have already been sent to the client.
//	outUIDs : Vector of all UIDs of resources that could potentially need to be sent across.
//	req : Object that defines what needs to transfered across.
//Returns the size of the vector after having UIDs of already sent resources removed, and puts the new UIDs in the outUIDs vector.
size_t GetNewUIDs(std::vector<avs::uid> & outUIDs, avs::GeometryRequesterBackendInterface * req)
{
	//Remove uids the requester has.
	for(auto it = outUIDs.begin(); it != outUIDs.end();)
	{
		if(req->hasResource(*it))
		{
			it = outUIDs.erase(it);
		}
		else
		{
			++it;
		}
	}

	return outUIDs.size();
}

GeometryEncoder::GeometryEncoder(const CasterSettings& settings)
	:settings(settings)
{}

unsigned char GeometryEncoder::GALU_code[] = { 0x01,0x00,0x80,0xFF };

avs::Result GeometryEncoder::encode(uint32_t timestamp, avs::GeometrySourceBackendInterface * src, avs::GeometryRequesterBackendInterface * req)
{
	queuedBuffer.clear();

	// The source backend will give us the data to encode.
	// What data it provides depends on the contents of the avs::GeometryRequesterBackendInterface object.

	//Encode data onto buffer, and then move it onto queuedBuffer.
	//Unless queueing the data would causes queuedBuffer to exceed the recommended buffer size, which will cause the data to stay in buffer until the next encode call.
	//Data may still be queued, and exceed the recommeneded size, if not queueing the data may leave it empty.

	//Queue what may have been left since last time, and keep queueing if there is still some space.
	bool keepQueueing = attemptQueueData();
	if(keepQueueing)
	{
		std::vector<avs::MeshNodeResources> meshNodeResources;
		std::vector<avs::LightNodeResources> lightNodeResources;

		req->getResourcesToStream(meshNodeResources, lightNodeResources);
		//Encode mesh nodes first, as they should be sent before lighting data.
		for(avs::MeshNodeResources meshResourceInfo : meshNodeResources)
		{
			if(!req->hasResource(meshResourceInfo.mesh_uid))
			{
				encodeMeshes(src, req, {meshResourceInfo.mesh_uid});

				keepQueueing = attemptQueueData();
				if(!keepQueueing) break;
			}

			for(avs::MaterialResources material : meshResourceInfo.materials)
			{
				if(GetNewUIDs(material.texture_uids, req) != 0)
				{
					for(avs::uid textureID : material.texture_uids)
					{
						encodeTextures(src, req, {textureID});

						keepQueueing = attemptQueueData();
						if(!keepQueueing) break;
					}

					if(!keepQueueing) break;
				}

				if(!req->hasResource(material.material_uid))
				{
					encodeMaterials(src, req, {material.material_uid});

					keepQueueing = attemptQueueData();
					if(!keepQueueing) break;
				}
			}
			if(!keepQueueing) break;

			if(!req->hasResource(meshResourceInfo.node_uid))
			{
				encodeNodes(src, req, {meshResourceInfo.node_uid});

				keepQueueing = attemptQueueData();
				if(!keepQueueing) break;
			}
		}

		for(avs::LightNodeResources lightResourceInfo : lightNodeResources)
		{
			if(!req->hasResource(lightResourceInfo.shadowmap_uid))
			{
				encodeTextures(src, req, {lightResourceInfo.shadowmap_uid});

				keepQueueing = attemptQueueData();
				if(!keepQueueing) break;
			}

			if(!req->hasResource(lightResourceInfo.node_uid))
			{
				encodeNodes(src, req, {lightResourceInfo.node_uid});

				keepQueueing = attemptQueueData();
				if(!keepQueueing) break;
			}
		}
	}

	// GALU to end.
	queuedBuffer.push_back(GALU_code[0]);
	queuedBuffer.push_back(GALU_code[1]);
	queuedBuffer.push_back(GALU_code[2]);
	queuedBuffer.push_back(GALU_code[3]);

	return avs::Result::OK;
}

avs::Result GeometryEncoder::mapOutputBuffer(void *& bufferPtr, size_t & bufferSizeInBytes)
{
	bufferSizeInBytes = queuedBuffer.size();
	bufferPtr = queuedBuffer.data(); 
	return avs::Result::OK;
}

avs::Result GeometryEncoder::unmapOutputBuffer()
{
	queuedBuffer.clear();
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeMeshes(avs::GeometrySourceBackendInterface * src, avs::GeometryRequesterBackendInterface * req, std::vector<avs::uid> missingUIDs)
{
	for(size_t h = 0; h < missingUIDs.size(); h++)
	{
		size_t oldBufferSize = buffer.size();

		putPayload(avs::GeometryPayloadType::Mesh);
		put((size_t)1);

		avs::uid uid = missingUIDs[h];
		put(uid);

		avs::Mesh mesh;
		src->getMesh(uid, mesh);

		put(mesh.primitiveArrays.size());

		std::set<avs::uid> accessors;
		for(avs::PrimitiveArray primitiveArray : mesh.primitiveArrays)
		{
			put(primitiveArray.attributeCount);
			put(primitiveArray.indices_accessor);
			put(primitiveArray.material);
			put(primitiveArray.primitiveMode);
			accessors.insert(primitiveArray.indices_accessor);
			for(size_t k = 0; k < primitiveArray.attributeCount; k++)
			{
				put(primitiveArray.attributes[k]);
				accessors.insert(primitiveArray.attributes[k].accessor);
			}
		}

		put(accessors.size());
		std::set<avs::uid> bufferViews;
		for(avs::uid accessorID : accessors)
		{
			avs::Accessor accessor = mesh.accessors[accessorID];
			put(accessorID);
			put(accessor.type);
			put(accessor.componentType);
			put(accessor.count);
			put(accessor.bufferView);
			bufferViews.insert(accessor.bufferView);
			put(accessor.byteOffset);
		}

		put(bufferViews.size());
		std::set<avs::uid> buffers;
		for(avs::uid bufferViewID : bufferViews)
		{
			avs::BufferView bufferView = mesh.bufferViews[bufferViewID];
			put(bufferViewID);
			put(bufferView.buffer);
			put(bufferView.byteOffset);
			put(bufferView.byteLength);
			put(bufferView.byteStride);
			buffers.insert(bufferView.buffer);
		}

		put(buffers.size());
		for(avs::uid bufferID : buffers)
		{
			avs::GeometryBuffer buffer = mesh.buffers[bufferID];
			put(bufferID);
			put(buffer.byteLength);
			put(buffer.data, buffer.byteLength);
		}

		req->encodedResource(uid);
	}
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeNodes(avs::GeometrySourceBackendInterface * src, avs::GeometryRequesterBackendInterface *req, std::vector<avs::uid> missingUIDs)
{
	//Place payload type onto the buffer.
	putPayload(avs::GeometryPayloadType::Node);

	put(missingUIDs.size());
	for (const avs::uid &uid : missingUIDs) 
	{
		avs::DataNode node;
		src->getNode(uid, node);

		put(uid);
		avs::Transform transform = node.transform;
		avs::ConvertTransform(avs::AxesStandard::UnrealStyle, req->axesStandard, transform);

		put(transform);
		put(node.data_uid);
		put(node.data_type); 
		put(node.materials.size());
		for (const auto& id : node.materials)
		{
			put(id);
		}
		put(node.childrenUids.size());
		for (const auto& id : node.childrenUids)
		{
			put(id);
		}

		req->encodedResource(uid); 
	}

	return avs::Result::OK;
}

void GeometryEncoder::putPayload(avs::GeometryPayloadType t)
{
	buffer.push_back(GALU_code[0]);
	buffer.push_back(GALU_code[1]);
	buffer.push_back(GALU_code[2]);
	buffer.push_back(GALU_code[3]);
	//Place payload type onto the buffer.
	put(t);
}

avs::Result GeometryEncoder::encodeTextures(avs::GeometrySourceBackendInterface * src, avs::GeometryRequesterBackendInterface * req, std::vector<avs::uid> missingUIDs)
{
	encodeTexturesBackend(src, req, missingUIDs);
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeMaterials(avs::GeometrySourceBackendInterface * src, avs::GeometryRequesterBackendInterface * req, std::vector<avs::uid> missingUIDs)
{
	//Push amount of materials.
	for(avs::uid uid : missingUIDs)
	{
		avs::Material outMaterial;

		if(src->getMaterial(uid, outMaterial))
		{
			putPayload(avs::GeometryPayloadType::Material);
			put((size_t)1);
			put(uid);

			size_t nameLength = outMaterial.name.length();

			//Push name length.
			put(nameLength);
			//Push name.
			put((uint8_t*)outMaterial.name.data(), nameLength);

			//Push base colour, and factor.
			put(outMaterial.pbrMetallicRoughness.baseColorTexture.index);
			put(outMaterial.pbrMetallicRoughness.baseColorTexture.texCoord);
			put(outMaterial.pbrMetallicRoughness.baseColorTexture.tiling.x);
			put(outMaterial.pbrMetallicRoughness.baseColorTexture.tiling.y);
			put(outMaterial.pbrMetallicRoughness.baseColorFactor.x);
			put(outMaterial.pbrMetallicRoughness.baseColorFactor.y);
			put(outMaterial.pbrMetallicRoughness.baseColorFactor.z);
			put(outMaterial.pbrMetallicRoughness.baseColorFactor.w);

			//Push metallic roughness, and factors.
			put(outMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index);
			put(outMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord);
			put(outMaterial.pbrMetallicRoughness.metallicRoughnessTexture.tiling.x);
			put(outMaterial.pbrMetallicRoughness.metallicRoughnessTexture.tiling.y);
			put(outMaterial.pbrMetallicRoughness.metallicFactor);
			put(outMaterial.pbrMetallicRoughness.roughnessFactor);

			//Push normal map, and scale.
			put(outMaterial.normalTexture.index);
			put(outMaterial.normalTexture.texCoord);
			put(outMaterial.normalTexture.tiling.x);
			put(outMaterial.normalTexture.tiling.y);
			put(outMaterial.normalTexture.scale);

			//Push occlusion texture, and strength.
			put(outMaterial.occlusionTexture.index);
			put(outMaterial.occlusionTexture.texCoord);
			put(outMaterial.occlusionTexture.tiling.x);
			put(outMaterial.occlusionTexture.tiling.y);
			put(outMaterial.occlusionTexture.strength);

			//Push emissive texture, and factor.
			put(outMaterial.emissiveTexture.index);
			put(outMaterial.emissiveTexture.texCoord);
			put(outMaterial.emissiveTexture.tiling.x);
			put(outMaterial.emissiveTexture.tiling.y);
			put(outMaterial.emissiveFactor.x);
			put(outMaterial.emissiveFactor.y);
			put(outMaterial.emissiveFactor.z);

			//Push extension amount.
			put(outMaterial.extensions.size());
			//Push extensions.
			for(const auto &extensionPair : outMaterial.extensions)
			{
				extensionPair.second->serialise(buffer);
			}

			//UIDs used by textures in material.
			std::vector<avs::uid> materialTexture_uids =
			{
				outMaterial.pbrMetallicRoughness.baseColorTexture.index,
				outMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index,
				outMaterial.normalTexture.index,
				outMaterial.occlusionTexture.index,
				outMaterial.emissiveTexture.index
			};

			//Array needs to be sorted for std::unique; we won't have many elements anyway.
			std::sort(materialTexture_uids.begin(), materialTexture_uids.end());
			//Shift data over duplicates, and erase.
			materialTexture_uids.erase(std::unique(materialTexture_uids.begin(), materialTexture_uids.end()), materialTexture_uids.end());
			//Shift data over 0s, and erase.
			materialTexture_uids.erase(std::remove(materialTexture_uids.begin(), materialTexture_uids.end(), 0), materialTexture_uids.end());
			
			//Only send textures that we have not already sent to the client.
			GetNewUIDs(materialTexture_uids, req);

			//Push amount of textures we are sending.
			put(materialTexture_uids.size());
			
			if(materialTexture_uids.size() != 0)
			{
				//Push textures.
				encodeTexturesBackend(src, req, materialTexture_uids);
			}

			//Flag we have encoded the material.
			req->encodedResource(uid);
		}
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeShadowMaps(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs)
{
	encodeTexturesBackend(src, req, missingUIDs, true);
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeTexturesBackend(avs::GeometrySourceBackendInterface * src, avs::GeometryRequesterBackendInterface * req, std::vector<avs::uid> missingUIDs, bool isShadowMap)
{
	for(avs::uid uid : missingUIDs)
	{
		avs::Texture outTexture;
		bool textureIsFound = false;
		if(isShadowMap)
			textureIsFound = src->getShadowMap(uid, outTexture);
		else
			textureIsFound = src->getTexture(uid, outTexture);

		if(textureIsFound)
		{
			size_t oldBufferSize = buffer.size();

			//Place payload type onto the buffer.
			putPayload(avs::GeometryPayloadType::Texture);
			//Push amount of textures we are sending.
			put((size_t)1);
			//Push identifier.
			put(uid);

			size_t nameLength = outTexture.name.length();

			//Push name length.
			put(nameLength);
			//Push name.
			put((uint8_t*)outTexture.name.data(), nameLength);

			//Push dimensions.
			put(outTexture.width);
			put(outTexture.height);

			//Push additional information.
			put(outTexture.depth);
			put(outTexture.bytesPerPixel);
			put(outTexture.arrayCount);
			put(outTexture.mipCount);

			//Push format.
			put(outTexture.format);
			put(outTexture.compression);

			//Push size, and data.
			put(outTexture.dataSize);
			put(outTexture.data, outTexture.dataSize);

			//Push sampler identifier.
			put(outTexture.sampler_uid);

			//Flag we have encoded the texture.
			req->encodedResource(uid);
		}
	}

	return avs::Result::OK;
}

bool GeometryEncoder::attemptQueueData()
{
	//If queueing the data will cause the queuedBuffer to exceed the cutoff size.
	if(buffer.size() + queuedBuffer.size() > settings.geometryBufferCutoffSize)
	{
		//Never leave queuedBuffer empty, if there is something to queue up (even if it is too large).
		if(queuedBuffer.size() == 0)
		{
			size_t position = queuedBuffer.size();
			queuedBuffer.resize(queuedBuffer.size() + buffer.size());

			memcpy(queuedBuffer.data() + position, buffer.data(), buffer.size());
			buffer.clear();
		}

		return false;
	}
	else
	{
		size_t position = queuedBuffer.size();
		queuedBuffer.resize(queuedBuffer.size() + buffer.size());

		memcpy(queuedBuffer.data() + position, buffer.data(), buffer.size());
		buffer.clear();

		return true;
	}
}
