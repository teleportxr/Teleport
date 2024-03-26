#include "GeometryEncoder.h"

#include <algorithm>
#include <set>

#include "libavstream/common.hpp"
#include "TeleportCore/AnimationInterface.h"

#include "ServerSettings.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/TextCanvas.h"
#include "GeometryStreamingService.h"
#include "GeometryStore.h"


using namespace teleport;
using namespace server;

//Clear a passed vector of UIDs that are believed to have already been sent to the client.
//	outUIDs : Vector of all UIDs of resources that could potentially need to be sent across.
//	req : Object that defines what needs to transfered across.
//Returns the size of the vector after having UIDs of already sent resources removed, and puts the new UIDs in the outUIDs vector.
size_t GetNewUIDs(std::vector<avs::uid>& outUIDs, avs::GeometryRequesterBackendInterface* req)
{
	//Remove uids the requester has.
	for (auto it = outUIDs.begin(); it != outUIDs.end();)
	{
		if (req->hasResource(*it))
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

GeometryEncoder::GeometryEncoder(const ServerSettings* settings, GeometryStreamingService* srv)
	: geometryStreamingService(srv),settings(settings)
{}

avs::Result GeometryEncoder::encode(uint64_t timestamp, avs::GeometryRequesterBackendInterface*)
{
	if (!geometryStreamingService || geometryStreamingService->getClientAxesStandard() == avs::AxesStandard::NotInitialized)
		return avs::Result::Failed;
	queuedBuffer.clear();
//	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	// The source backend will give us the data to encode.
	// What data it provides depends on the contents of the avs::GeometryRequesterBackendInterface object.

	//Encode data onto buffer, and then move it onto queuedBuffer.
	//Unless queueing the data would causes queuedBuffer to exceed the recommended buffer size, which will cause the data to stay in buffer until the next encode call.
	//Data may still be queued, and exceed the recommeneded size, if not queueing the data may leave it empty.

	//Queue what may have been left since last time, and keep queueing if there is still some space.
	bool keepQueueing = attemptQueueData();
	if (keepQueueing)
	{
		std::set<avs::uid> nodeIDsToStream;
		std::vector<avs::MeshNodeResources> meshNodeResources;
		std::vector<avs::LightNodeResources> lightNodeResources;
		std::vector<avs::uid> textCanvas_uids;
		std::vector<avs::uid> font_uids;
		std::set<avs::uid> genericTexturesToStream;

		geometryStreamingService->getResourcesToStream(nodeIDsToStream, meshNodeResources, lightNodeResources, genericTexturesToStream
			, textCanvas_uids, font_uids, minimumPriority);

		for (avs::uid nodeID : nodeIDsToStream)
		{
			if (!geometryStreamingService->hasResource(nodeID))
			{
				encodeNodes(geometryStreamingService, { nodeID });

				keepQueueing = attemptQueueData();
				if (!keepQueueing)
				{
					break;
				}
			}
		}

		//Encode mesh nodes first, as they should be sent before lighting data.
		for (avs::MeshNodeResources meshResourceInfo : meshNodeResources)
		{
			if(meshResourceInfo.mesh_uid!=0&&!geometryStreamingService->hasResource(meshResourceInfo.mesh_uid))
			{
				encodeMeshes(geometryStreamingService, { meshResourceInfo.mesh_uid });

				keepQueueing = attemptQueueData();
				if (!keepQueueing)
				{
					break;
				}
			}
			if (meshResourceInfo.skeletonAssetID != 0&&!geometryStreamingService->hasResource(meshResourceInfo.skeletonAssetID))
			{
				encodeSkeleton(geometryStreamingService, meshResourceInfo.skeletonAssetID);

				keepQueueing = attemptQueueData();
				if (!keepQueueing)
				{
					break;
				}
			}

			for (avs::uid animationID : meshResourceInfo.animationIDs)
			{
				if (animationID!=0&&!geometryStreamingService->hasResource(animationID))
				{
					encodeAnimation(geometryStreamingService, animationID);

					keepQueueing = attemptQueueData();
					if (!keepQueueing)
					{
						break;
					}
				}
			}
			if (!keepQueueing)
			{
				break;
			}

			for (avs::MaterialResources material : meshResourceInfo.materials)
			{
				if (material.material_uid!=0&&!geometryStreamingService->hasResource(material.material_uid))
				{
					encodeMaterials(geometryStreamingService, { material.material_uid });
					keepQueueing = attemptQueueData();
					if (!keepQueueing)
					{
						break;
					}
				}
				if (GetNewUIDs(material.texture_uids, geometryStreamingService) != 0)
				{
					for (avs::uid textureID : material.texture_uids)
					{
						if(textureID==0)
							continue;
						encodeTextures(geometryStreamingService, { textureID });
						keepQueueing = attemptQueueData();
						if (!keepQueueing)
						{
							break;
						}
					}
					if (!keepQueueing)
					{
						break;
					}
				}
			}
			if (!keepQueueing)
			{
				break;
			}

			if (!geometryStreamingService->hasResource(meshResourceInfo.node_uid))
			{
				encodeNodes(geometryStreamingService, { meshResourceInfo.node_uid });

				keepQueueing = attemptQueueData();
				if (!keepQueueing)
				{
					break;
				}
			}
		}

		for (avs::LightNodeResources lightResourceInfo : lightNodeResources)
		{
			if (lightResourceInfo.shadowmap_uid)
			{
				if (!geometryStreamingService->hasResource(lightResourceInfo.shadowmap_uid))
				{
					encodeTextures(geometryStreamingService, { lightResourceInfo.shadowmap_uid });

					keepQueueing = attemptQueueData();
					if (!keepQueueing)
					{
						break;
					}
				}
			}

			if (!geometryStreamingService->hasResource(lightResourceInfo.node_uid))
			{
				encodeNodes(geometryStreamingService, { lightResourceInfo.node_uid });

				keepQueueing = attemptQueueData();
				if (!keepQueueing)
				{
					break;
				}
			}
		}

		//Encode mesh nodes first, as they should be sent before lighting data.
		for (avs::uid texture_uid : genericTexturesToStream)
		{
			if (geometryStreamingService->hasResource(texture_uid))
				continue;
			encodeTextures(geometryStreamingService, { texture_uid });
			keepQueueing = attemptQueueData();
			if (!keepQueueing)
			{
				break;
			}
		}
		for (avs::uid font_uid : font_uids)
		{
			if (geometryStreamingService->hasResource(font_uid))
				continue;
			encodeFontAtlas(font_uid);
			keepQueueing = attemptQueueData();
			if (!keepQueueing)
			{
				break;
			}
		}
		for (avs::uid canvas_uid : textCanvas_uids)
		{
			if (geometryStreamingService->hasResource(canvas_uid))
				continue;
			encodeTextCanvas(canvas_uid);
			keepQueueing = attemptQueueData();
			if (!keepQueueing)
			{
				break;
			}
		}

	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes)
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

void GeometryEncoder::setMinimumPriority(int32_t p)
{
	minimumPriority = p;
}

avs::Result GeometryEncoder::encodeMeshes(avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs)
{
	GeometryStore* geometryStore = &GeometryStore::GetInstance();
	for (avs::uid uid : missingUIDs)
	{
		if(uid==0)
			continue;
		const avs::Mesh* mesh = geometryStore->getMesh(uid, geometryStreamingService->getClientAxesStandard());
		const avs::CompressedMesh* compressedMesh = geometryStore->getCompressedMesh(uid, geometryStreamingService->getClientAxesStandard());
		putPayloadType(avs::GeometryPayloadType::Mesh,uid);
		if (compressedMesh && compressedMesh->meshCompressionType != avs::MeshCompressionType::NONE)
		{
			uint64_t lowest_accessor = 0xFFFFFFFFFFFFFFFF, highest_accessor = 0;
			compressedMesh->GetAccessorRange(lowest_accessor, highest_accessor);
			uint64_t accessor_subtract = lowest_accessor;
			if(compressedMesh->meshCompressionType==avs::MeshCompressionType::DRACO)
				put(avs::MeshCompressionType::DRACO_VERSIONED);
			else
				put(compressedMesh->meshCompressionType);
			uint16_t version=1;
			put(version);
			static const int32_t DRACO_COMPRESSED_MESH_VERSION_NUMBER = 1;
			put(DRACO_COMPRESSED_MESH_VERSION_NUMBER);
			size_t nameLength = compressedMesh->name.length();
			put(nameLength);
			put((uint8_t*)compressedMesh->name.data(), nameLength);

			// skin binding. Not included in draco Meshes, just Scenes. So we add it here:
			std::tuple<const uint8_t*,size_t> data=mesh->GetDataFromAccessor(mesh->inverseBindMatricesAccessorID);
			size_t inv_bind_datasize=std::get<size_t>(data);
			put(inv_bind_datasize);
			if(inv_bind_datasize>0)
			{
				const uint8_t* inv_bind_data=std::get<const uint8_t*>(data);
				put(inv_bind_data, inv_bind_datasize);
			}

			size_t num_elements = compressedMesh->subMeshes.size();
			put((uint32_t)num_elements);
			for (size_t i = 0; i < num_elements; i++)
			{
				auto& subMesh = compressedMesh->subMeshes[i];
				put(subMesh.indices_accessor - accessor_subtract);
				put(subMesh.material);
				put(subMesh.first_index);
				put(subMesh.num_indices);
				size_t numAttrs = subMesh.attributeSemantics.size();
				put(numAttrs);
				for (auto a : subMesh.attributeSemantics)
				{
					put((int32_t)a.first);
					put((uint8_t)a.second);
				}
				size_t bufferSize = subMesh.buffer.size();
				if (bufferSize == 0)
				{
					TELEPORT_INTERNAL_CERR("Empty submesh buffer for {0}", compressedMesh->name.c_str());
				}

				put(bufferSize);
				put((uint8_t*)subMesh.buffer.data(), bufferSize);
			}
		}
		if (!compressedMesh || compressedMesh->meshCompressionType == avs::MeshCompressionType::NONE)
		{
			TELEPORT_CERR << "Mesh encoding error! Mesh " << uid << " MeshCompressionType::NONE is not supported!\n";
			continue;
		}
		// Actual size is now known so update payload size
		putPayloadSize();

		geometryStreamingService->encodedResource(uid);
	}
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeNodes(avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs)
{
	GeometryStore* geometryStore = &GeometryStore::GetInstance();
	//Place payload type onto the buffer.
	for (int i = 0; i < (int)missingUIDs.size(); i++)
	{
		avs::uid uid = missingUIDs[i];
		avs::Node* node = geometryStore->getNode(uid);
		if (!node)
		{
			TELEPORT_CERR << "PipelineNode encoding error! Node_" << uid << " does not exist!\n";
			missingUIDs.erase(missingUIDs.begin() + i);
			i--;
		}
	}
	for (const avs::uid& uid : missingUIDs)
	{
		avs::Node* node = geometryStore->getNode(uid);
		putPayloadType(avs::GeometryPayloadType::Node,uid);
		//Push name length.
		size_t nameLength = node->name.length();
		put(nameLength);
		//Push name.
		put((uint8_t*)node->name.data(), nameLength);

		avs::Transform localTransform = node->localTransform;
		avs::ConvertTransform(settings->serverAxesStandard, geometryStreamingService->getClientAxesStandard(), localTransform);

		put(localTransform);
		put((uint8_t)(node->stationary));

		put(node->holder_client_id);

		put(node->priority);
		put(node->data_uid);
		put(node->data_type);

		put(node->skeletonNodeID);
		put(node->joint_indices.size());
		if(node->joint_indices.size())
		{
			for (int16_t index : node->joint_indices)
			{
				put(index);
			}
		}
		put(node->parentID);

		put(node->animations.size());
		for (avs::uid id : node->animations)
		{
			put(id);
		}
		// If the node's priority is less than the *client's* minimum, we don't want
		// to send its mesh.
		if (node->data_type == avs::NodeDataType::Mesh)
		{
			put(node->materials.size());
			for (avs::uid id : node->materials)
			{
				put(id);
			}
			put(node->renderState.lightmapScaleOffset);
			put(node->renderState.globalIlluminationUid);
		}
		else
		{
			TELEPORT_ASSERT(node->materials.size() == 0);
		}

		if (node->data_type == avs::NodeDataType::Light)
		{
			put(node->lightColour);
			put(node->lightRadius);
			put(node->lightRange);
			vec3 lightDirection = node->lightDirection;
			avs::ConvertPosition(settings->serverAxesStandard, geometryStreamingService->getClientAxesStandard(), lightDirection);
			put(lightDirection);
			put(node->lightType);
		}
		if (node->data_type == avs::NodeDataType::TextCanvas)
		{
			// nothing node-specific to add at present.
		}
		if (node->data_type == avs::NodeDataType::Skeleton)
		{
		}
		geometryStreamingService->encodedResource(uid);
	}

	// Actual size is now known so update payload size
	putPayloadSize();

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeSkeleton(avs::GeometryRequesterBackendInterface*, avs::uid skeletonID)
{
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	const avs::Skeleton* skeleton = geometryStore->getSkeleton(skeletonID, geometryStreamingService->getClientAxesStandard());
	if(skeleton)
	{
		putPayloadType(avs::GeometryPayloadType::Skeleton,skeletonID);
		//Push name length.
		size_t nameLength = skeleton->name.length();
		put(nameLength);
		//Push name.
		put((uint8_t*)skeleton->name.data(), nameLength);
		auto findIndex = [](std::vector<avs::uid> v, avs::uid u)
		{
			auto j = std::find(v.begin(), v.end(), u);
			int16_t index = (int16_t)std::distance(v.begin(), j);
			if(index==v.size())
				index=-1;
			return index;
		};
		put(true);
		put(skeleton->boneIDs.size());
		for (int i = 0; i <(int) skeleton->boneIDs.size(); i++)
		{
			avs::Node* node = geometryStore->getNode(skeleton->boneIDs[i]);
			avs::Transform localTransform = node->localTransform;
			avs::ConvertTransform(settings->serverAxesStandard, geometryStreamingService->getClientAxesStandard(), localTransform);
			put(skeleton->boneIDs[i]);
			int16_t parentIndex = findIndex(skeleton->boneIDs, node->parentID);
			put(parentIndex);
			put(localTransform);
			size_t nameLength = node->name.length();
			put(nameLength);
			//Push name.
			put((uint8_t*)node->name.data(), nameLength);
		}
		put(skeleton->skeletonTransform);

		putPayloadSize();
		geometryStreamingService->encodedResource(skeletonID);
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeAnimation(avs::GeometryRequesterBackendInterface*, avs::uid animationID)
{
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	const teleport::core::Animation* animation = geometryStore->getAnimation(animationID, geometryStreamingService->getClientAxesStandard());
	if (animation)
	{
		putPayloadType(avs::GeometryPayloadType::Animation,animationID);

		//Push name length.
		size_t nameLength = animation->name.length();
		put(nameLength);
		//Push name.
		put((uint8_t*)animation->name.data(), nameLength);

		put(animation->boneKeyframes.size());
		for (const teleport::core::TransformKeyframeList& transformKeyframe : animation->boneKeyframes)
		{
			put(transformKeyframe.boneIndex);

			encodeVector3Keyframes(transformKeyframe.positionKeyframes);
			encodeVector4Keyframes(transformKeyframe.rotationKeyframes);
		}

		putPayloadSize();
		geometryStreamingService->encodedResource(animationID);
	}

	return avs::Result::OK;
}

void GeometryEncoder::putPayloadType(avs::GeometryPayloadType t,avs::uid uid)
{
	prevBufferSize = buffer.size();

	// Add placeholder for the payload size 
	put(size_t(sizeof(avs::GeometryPayloadType)));

	// Place payload type onto the buffer.
	put(t);
	put(uid);
}

void GeometryEncoder::putPayloadSize()
{
	if (!buffer.size())
	{
		prevBufferSize = 0;
		return;
	}

	size_t payloadSize = buffer.size() - prevBufferSize - sizeof(size_t);

	// prevBufferSize will be the index where the payload size placeholder was added
	replace(prevBufferSize, payloadSize);

	avs::GeometryPayloadType type;
	memcpy(&type,(buffer.data()+prevBufferSize+sizeof(size_t)),sizeof(type));
	std::cout<<" payloadSize "<<payloadSize<<" for "<<stringOf(type)<<"\n";

	prevBufferSize = 0;
}

avs::Result GeometryEncoder::encodeFontAtlas(avs::uid uid)
{
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	const teleport::core::FontAtlas* fontAtlas = geometryStore->getFontAtlas(uid);

	if (!fontAtlas)
		return avs::Result::Failed;
	putPayloadType(avs::GeometryPayloadType::FontAtlas,uid);
	put(fontAtlas->font_texture_uid);
	if (fontAtlas->fontMaps.size() > 255)
	{
		return avs::Result::Failed;
	}
	uint8_t numMaps = (uint8_t)fontAtlas->fontMaps.size();
	put(numMaps);
	for (auto m : fontAtlas->fontMaps)
	{
		put(m.first);
		auto& fontMap = m.second;
		put(fontMap.lineHeight);
		uint16_t numGlyphs = (uint16_t)fontMap.glyphs.size();
		if ((size_t)numGlyphs != fontMap.glyphs.size())
		{
			return avs::Result::Failed;
		}
		put(numGlyphs);
		for (size_t j = 0; j < fontMap.glyphs.size(); j++)
		{
			auto& glyph = fontMap.glyphs[j];
			put(glyph);
		}
	}
	// Actual size is now known so update payload size
	putPayloadSize();
	//Flag we have encoded the material.
	geometryStreamingService->encodedResource(uid);
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeTextCanvas(avs::uid uid)
{
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	const teleport::core::TextCanvas* textCanvas = geometryStore->getTextCanvas(uid);

	if (!textCanvas)
		return avs::Result::Failed;
	putPayloadType(avs::GeometryPayloadType::TextCanvas,uid);
	put(textCanvas->font_uid);
	put(textCanvas->size);
	put(textCanvas->lineHeight);
	put(textCanvas->width);
	put(textCanvas->height);
	put(textCanvas->colour);
	put(textCanvas->text.length());
	put((const uint8_t*)textCanvas->text.data(), textCanvas->text.length());
	// Actual size is now known so update payload size
	putPayloadSize();
	//Flag we have encoded the material.
	geometryStreamingService->encodedResource(uid);
	return avs::Result::OK;
}


avs::Result GeometryEncoder::encodeTextures(avs::GeometryRequesterBackendInterface*
	, std::vector<avs::uid> missingUIDs)
{

	encodeTexturesBackend(geometryStreamingService, missingUIDs);
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeMaterials(avs::GeometryRequesterBackendInterface*
	, std::vector<avs::uid> missingUIDs)
{
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	auto renderingFeatures = geometryStreamingService->getClientRenderingFeatures();
	
	for (avs::uid uid : missingUIDs)
	{
		avs::Material* material = geometryStore->getMaterial(uid);

		if (material)
		{
			//UIDs used by textures in material. DON'T SEND THESE HERE!
			std::vector<avs::uid> materialTexture_uids;// = material->GetTextureUids();
			for (auto u : materialTexture_uids)
			{
				if (!geometryStore->getTexture(u))
				{
					TELEPORT_CERR << "Material " << material->name.c_str() << " points to " << u << " which is not a texture.\n";
					continue;
				}
			}
			putPayloadType(avs::GeometryPayloadType::Material,uid);

			size_t nameLength = material->name.length();

			//Push name length.
			put(nameLength);
			//Push name.
			put((uint8_t*)material->name.data(), nameLength);

			put(material->materialMode);

			//Push base colour, and factor.
			put(material->pbrMetallicRoughness.baseColorTexture.index);
			put(material->pbrMetallicRoughness.baseColorTexture.texCoord);
			put(material->pbrMetallicRoughness.baseColorTexture.tiling.x);
			put(material->pbrMetallicRoughness.baseColorTexture.tiling.y);
			put(material->pbrMetallicRoughness.baseColorFactor.x);
			put(material->pbrMetallicRoughness.baseColorFactor.y);
			put(material->pbrMetallicRoughness.baseColorFactor.z);
			put(material->pbrMetallicRoughness.baseColorFactor.w);

			//Push metallic roughness, and factors.
			put(material->pbrMetallicRoughness.metallicRoughnessTexture.index);
			put(material->pbrMetallicRoughness.metallicRoughnessTexture.texCoord);
			put(material->pbrMetallicRoughness.metallicRoughnessTexture.tiling.x);
			put(material->pbrMetallicRoughness.metallicRoughnessTexture.tiling.y);
			put(material->pbrMetallicRoughness.metallicFactor);
			put(material->pbrMetallicRoughness.roughnessMultiplier);
			put(material->pbrMetallicRoughness.roughnessOffset);

			//Push normal map, and scale.
			// TODO Note: correspondence between these handshake feature checks and those at GeometryStreamingService::GetMeshNodeResources!
			if (renderingFeatures.normals)
				put(material->normalTexture.index);
			else
				put(avs::uid(0));
			put(material->normalTexture.texCoord);
			put(material->normalTexture.tiling.x);
			put(material->normalTexture.tiling.y);
			put(material->normalTexture.scale);

			//Push occlusion texture, and strength.
			// Note, if AO is not supported, we MUST put a zero, or the client will request a missing texture that never arrives.
			if (renderingFeatures.ambientOcclusion)
				put(material->occlusionTexture.index);
			else
				put(avs::uid(0));
			put(material->occlusionTexture.texCoord);
			put(material->occlusionTexture.tiling.x);
			put(material->occlusionTexture.tiling.y);
			put(material->occlusionTexture.strength);

			//Push emissive texture, and factor.
			put(material->emissiveTexture.index);
			put(material->emissiveTexture.texCoord);
			put(material->emissiveTexture.tiling.x);
			put(material->emissiveTexture.tiling.y);
			put(material->emissiveFactor.x);
			put(material->emissiveFactor.y);
			put(material->emissiveFactor.z);

			put(material->doubleSided);
			put(material->lightmapTexCoordIndex);

			//Push extension amount.
			put(material->extensions.size());
			//Push extensions.
			for (const auto& extensionPair : material->extensions)
			{
				extensionPair.second->serialise(buffer);
			}


			//Array needs to be sorted for std::unique; we won't have many elements anyway.
			std::sort(materialTexture_uids.begin(), materialTexture_uids.end());
			//Shift data over duplicates, and erase.
			materialTexture_uids.erase(std::unique(materialTexture_uids.begin(), materialTexture_uids.end()), materialTexture_uids.end());
			//Shift data over 0s, and erase.
			materialTexture_uids.erase(std::remove(materialTexture_uids.begin(), materialTexture_uids.end(), 0), materialTexture_uids.end());

			//Only send textures that we have not already sent to the client.
			GetNewUIDs(materialTexture_uids, geometryStreamingService);

			//Push amount of textures we are sending.
			put(materialTexture_uids.size());

			// Actual size is now known so update payload size
			putPayloadSize();

			if (materialTexture_uids.size() != 0)
			{
				//Push textures.
				encodeTexturesBackend(geometryStreamingService, materialTexture_uids);
			}

			//Flag we have encoded the material.
			geometryStreamingService->encodedResource(uid);
		}
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeShadowMaps(avs::GeometryRequesterBackendInterface*, std::vector<avs::uid> missingUIDs)
{
	encodeTexturesBackend(geometryStreamingService, missingUIDs, true);
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeTexturesBackend(avs::GeometryRequesterBackendInterface*, std::vector<avs::uid> missingUIDs, bool)
{
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	for (avs::uid uid : missingUIDs)
	{
		avs::Texture* texture;

		texture = geometryStore->getTexture(uid);
		if (texture)
		{
			if(texture->dataSize==0)
			{
				TELEPORT_CERR << "Trying to send a zero-size texture. Never do this!\n";
				continue;
			}
			if (texture->compression == avs::TextureCompression::UNCOMPRESSED)
			{
				TELEPORT_CERR << "Trying to send uncompressed texture. Never do this!\n";
				continue;
			}
			//size_t oldBufferSize = buffer.size();

			//Place payload type onto the buffer.
			putPayloadType(avs::GeometryPayloadType::Texture,uid);

			size_t nameLength = texture->name.length();

			//Push name length.
			put(nameLength);
			//Push name.
			put((uint8_t*)texture->name.data(), nameLength);

			// TODO: make this a more generic texture type.
			put(texture->cubemap);

			//Push dimensions.
			put(texture->width);
			put(texture->height);

			//Push additional information.
			put(texture->depth);
			put(texture->bytesPerPixel);
			put(texture->arrayCount);
			put(texture->mipCount);

			//Push format.
			put(texture->format);
			put(texture->compression);

			//Value scale - brightness number to scale the final texel by.
			put(texture->valueScale);

			//Push size, and data.
			put(texture->dataSize);
			put(texture->data, texture->dataSize);

			//Push sampler identifier.
			put(texture->sampler_uid);

			// Actual size is now known so update payload size
			putPayloadSize();

			//Flag we have encoded the texture.
			geometryStreamingService->encodedResource(uid);
		}
		else
		{
			//DEBUG_BREAK_ONCE("Missing texture");
			TELEPORT_CERR << "Trying to encode texture " << uid << " but it is not there.\n";
		}
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeFloatKeyframes(const std::vector<teleport::core::FloatKeyframe>& keyframes)
{
	put(keyframes.size());
	for (const teleport::core::FloatKeyframe& keyframe : keyframes)
	{
		put(keyframe.time);
		put(keyframe.value);
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeVector3Keyframes(const std::vector<teleport::core::Vector3Keyframe>& keyframes)
{
	put(keyframes.size());
	for (const teleport::core::Vector3Keyframe& keyframe : keyframes)
	{
		put(keyframe.time);
		put(keyframe.value);
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeVector4Keyframes(const std::vector<teleport::core::Vector4Keyframe>& keyframes)
{
	put(keyframes.size());
	for (const teleport::core::Vector4Keyframe& keyframe : keyframes)
	{
		put(keyframe.time);
		put(keyframe.value);
	}

	return avs::Result::OK;
}

size_t GeometryEncoder::put(const uint8_t* data, size_t count)
{
	size_t pos = buffer.size();
	if(count>0)
	{
		buffer.resize(buffer.size() + count);
		memcpy(buffer.data() + pos, data, count);
	}
#if TELEPORT_DEBUG_GEOMETRY_MESSAGES
	if (count >= settings->geometryBufferCutoffSize)
	{
		TELEPORT_CERR << "Data too big for geometry buffer cutoff size.\n";
	}
#endif
	return pos;
}

bool GeometryEncoder::attemptQueueData()
{
	//If queueing the data will cause the queuedBuffer to exceed the cutoff size.
	if (buffer.size() + queuedBuffer.size() > settings->geometryBufferCutoffSize)
	{
		//Never leave queuedBuffer empty, if there is something to queue up (even if it is too large).
		if (queuedBuffer.size() == 0)
		{
			size_t position = queuedBuffer.size();
			queuedBuffer.resize(queuedBuffer.size() + buffer.size());

			memcpy(queuedBuffer.data() + position, buffer.data(), buffer.size());
			buffer.clear();
		}

		return false;
	}
	else if (buffer.size())
	{
		size_t position = queuedBuffer.size();
		queuedBuffer.resize(queuedBuffer.size() + buffer.size());

		memcpy(queuedBuffer.data() + position, buffer.data(), buffer.size());
		buffer.clear();

		return true;
	}
	return true;
}
