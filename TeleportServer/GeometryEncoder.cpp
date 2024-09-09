#include "GeometryEncoder.h"

#include <algorithm>
#include <set>

#include "libavstream/common.hpp"
#include "TeleportCore/Animation.h"

#include "ServerSettings.h"

#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/TextCanvas.h"
#include "TeleportCore/Logging.h"
#include "TeleportCore/Profiling.h"
#include "GeometryStreamingService.h"
#include "GeometryStore.h"
#include "ClientManager.h"

#pragma optimize("", off)
using namespace teleport;
using namespace server;
using std::string;
using std::vector;

GeometryEncoder::GeometryEncoder( GeometryStreamingService *srv, avs::uid clid)
	: geometryStreamingService(srv), clientID(clid)
{}

avs::Result GeometryEncoder::encode(uint64_t timestamp)
{
	TELEPORT_PROFILE_AUTOZONE;
	if (!geometryStreamingService || geometryStreamingService->getClientAxesStandard() == avs::AxesStandard::NotInitialized)
		return avs::Result::Failed;
	queuedBuffer.clear();
//	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	// The source backend will give us the data to encode.
	// What data it provides depends on the contents of the avs::GeometryRequesterBackendInterface object.

	//Encode data onto buffer, and then move it onto queuedBuffer.
	//Unless queueing the data would causes queuedBuffer to exceed the recommended buffer size, which will cause the data to stay in buffer until the next encode call.
	//Data may still be queued, and exceed the recommended size, if not queueing the data may leave it empty.

	//Queue what may have been left since last time, and keep queueing if there is still some space.
	bool keepQueueing = attemptQueueData();
	if (!keepQueueing)
		return avs::Result::OK;
	std::set<avs::uid> nodeIDsToStream;
	std::set<avs::uid> genericTexturesToStream;
	std::set<avs::uid> meshes;
	std::set<avs::uid> materials;
	std::set<avs::uid> textures;
	std::set<avs::uid> skeletons;
	std::set<avs::uid> bones;
	std::set<avs::uid> animations;
	std::set<avs::uid> textCanvases;
	std::set<avs::uid> fontAtlases;

	int32_t minimumPriority = 0;
	auto *clientSettings = ClientManager::instance().GetClientSettings(clientID);
	minimumPriority=clientSettings->minimumNodePriority;
	geometryStreamingService->updateResourcesToStream(minimumPriority);
	geometryStreamingService->getResourcesToStream(nodeIDsToStream, genericTexturesToStream,meshes,materials,textures,skeletons,bones,animations,textCanvases,fontAtlases);

	for (avs::uid nodeID : nodeIDsToStream)
	{
		TELEPORT_LOG_INTERNAL("Sending node {0}", nodeID);
		encodeNodes( { nodeID });

		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}

	//Encode mesh nodes first, as they should be sent before lighting data.
	for (avs::uid u : meshes)
	{
		encodeMeshes( { u });
		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}
	for (avs::uid u : skeletons)
	{
		encodeSkeleton( u);

		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}

	for (avs::uid u : animations)
	{
		encodeAnimation( u);
		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}
	if (!keepQueueing)
		return avs::Result::OK;
	for (avs::uid u : materials)
	{
		encodeMaterials( { u });
		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}
	if (!keepQueueing)
	{
		return avs::Result::OK;
	}
	/*
	for (avs::uid lightResourceInfo : lights)
	{
		if (lightResourceInfo.shadowmap_uid)
		{
			{
				encodeTextures( { lightResourceInfo.shadowmap_uid });

				keepQueueing = attemptQueueData();
				if (!keepQueueing)
				{
					break;
				}
			}
		}

		{
			encodeNodes( { lightResourceInfo.node_uid });

			keepQueueing = attemptQueueData();
			if (!keepQueueing)
			{
				break;
			}
		}
	}*/

	//Encode mesh nodes first, as they should be sent before lighting data.
	for (avs::uid texture_uid : genericTexturesToStream)
	{
		encodeTextures( { texture_uid });
		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}
	for (avs::uid texture_uid : textures)
	{
		encodeTextures( { texture_uid });
		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}
	for (avs::uid font_uid : fontAtlases)
	{
		encodeFontAtlas(font_uid);
		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
		}
	}
	for (avs::uid canvas_uid : textCanvases)
	{
		encodeTextCanvas(canvas_uid);
		keepQueueing = attemptQueueData();
		if (!keepQueueing)
		{
			break;
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

avs::Result GeometryEncoder::encodeMeshes( std::vector<avs::uid> missingUIDs)
{
	GeometryStore* geometryStore = &GeometryStore::GetInstance();
	for (avs::uid uid : missingUIDs)
	{
		if(uid==0)
			continue;
		const avs::Mesh* mesh = geometryStore->getMesh(uid, geometryStreamingService->getClientAxesStandard());
		const avs::CompressedMesh *compressedMesh = geometryStore->getCompressedMesh(uid, geometryStreamingService->getClientAxesStandard());
		if (!compressedMesh || compressedMesh->meshCompressionType == avs::MeshCompressionType::NONE)
		{
			TELEPORT_CERR << "Mesh encoding error! Mesh " << uid << " MeshCompressionType::NONE is not supported!\n";
			continue;
		}
		putPayloadType(avs::GeometryPayloadType::Mesh,uid);
		if (compressedMesh && compressedMesh->meshCompressionType != avs::MeshCompressionType::NONE)
		{
			uint64_t lowest_accessor = 0xFFFFFFFFFFFFFFFF, highest_accessor = 0;
			compressedMesh->GetAccessorRange(lowest_accessor, highest_accessor);
			uint64_t accessor_subtract = lowest_accessor;
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
				//put(subMesh.indices_accessor - accessor_subtract);
				//put(subMesh.material);
				//put(subMesh.first_index);
				//put(subMesh.num_indices);
				/*size_t numAttrs = subMesh.attributeSemantics.size();
				put(numAttrs);
				for (auto a : subMesh.attributeSemantics)
				{
					put((int32_t)a.first);
					put((uint8_t)a.second);
				}*/
				size_t bufferSize = subMesh.buffer.size();
				if (bufferSize == 0)
				{
					TELEPORT_INTERNAL_CERR("Empty submesh buffer for {0}", compressedMesh->name.c_str());
				}

				put(bufferSize);
				put((uint8_t*)subMesh.buffer.data(), bufferSize);
			}
		}
		// Actual size is now known so update payload size
		putPayloadSize(uid);

		geometryStreamingService->encodedResource(uid);
	}
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeNodes( std::vector<avs::uid> missingUIDs)
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
		avs::ConvertTransform(serverSettings.serverAxesStandard, geometryStreamingService->getClientAxesStandard(), localTransform);

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
			//put(node->renderState.lightmapTextureCoordinate);
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
			avs::ConvertPosition(serverSettings.serverAxesStandard, geometryStreamingService->getClientAxesStandard(), lightDirection);
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
		if (node->data_type == avs::NodeDataType::Link)
		{
			size_t urlLength = node->url.length();
			put(urlLength);
			put((uint8_t *)node->url.data(), urlLength);
			size_t queryLength = node->query_url.length();
			put(queryLength);
			put((uint8_t *)node->query_url.data(), queryLength);

		}
		geometryStreamingService->encodedResource(uid);
	}

	// Actual size is now known so update payload size
	if (missingUIDs.size() == 1)
		putPayloadSize(missingUIDs[0]);
	else
	putPayloadSize(0);

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeSkeleton( avs::uid skeletonID)
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
		put(skeleton->boneIDs.size());
		put((const uint8_t *)skeleton->boneIDs.data(), sizeof(avs::uid) * skeleton->boneIDs.size());
	/*	for (int i = 0; i <(int) skeleton->boneIDs.size(); i++)
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
		}*/
		put(skeleton->skeletonTransform);

		putPayloadSize(skeletonID);
		geometryStreamingService->encodedResource(skeletonID);
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeAnimation( avs::uid animationID)
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
		put(animation->duration);
		put(animation->boneKeyframes.size());
		for (const teleport::core::TransformKeyframeList& transformKeyframe : animation->boneKeyframes)
		{
			put(transformKeyframe.boneIndex);

			encodeVector3Keyframes(transformKeyframe.positionKeyframes);
			encodeVector4Keyframes(transformKeyframe.rotationKeyframes);
		}

		putPayloadSize(animationID);
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
	// Put the resource uid onto the buffer.
	put(uid);
}

void GeometryEncoder::putPayloadSize(avs::uid uid)
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
	//TELEPORT_LOG("GeometryEncoder put payload: {0} for {1} {2}", payloadSize, stringOf(type), uid);

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
	putPayloadSize(uid);
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
	putPayloadSize(uid);
	//Flag we have encoded the material.
	geometryStreamingService->encodedResource(uid);
	return avs::Result::OK;
}


avs::Result GeometryEncoder::encodeTextures( std::vector<avs::uid> missingUIDs)
{
	encodeTexturesBackend( missingUIDs);
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeMaterials( std::vector<avs::uid> missingUIDs)
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
			geometryStreamingService->GetNewUIDs(materialTexture_uids);

			//Push amount of textures we are sending.
			put(materialTexture_uids.size());

			// Actual size is now known so update payload size
			putPayloadSize(uid);

			if (materialTexture_uids.size() != 0)
			{
				//Push textures.
				encodeTexturesBackend( materialTexture_uids);
			}

			//Flag we have encoded the material.
			geometryStreamingService->encodedResource(uid);
		}
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeShadowMaps( std::vector<avs::uid> missingUIDs)
{
	encodeTexturesBackend( missingUIDs, true);
	return avs::Result::OK;
}
// don't send more than a Mb inline:
#define INLINE_DATA_THRESHOLD_KB (1024)

avs::Result GeometryEncoder::encodeTexturePointer( avs::uid uid)
{
	//Place payload type onto the buffer.
	putPayloadType(avs::GeometryPayloadType::TexturePointer,uid);
	
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	const ExtractedTexture* wrappedTexture= geometryStore->getWrappedTexture(uid);
	string path=geometryStore->UidToPath(uid);;
	if(!path.length())
		return avs::Result::Failed;
	string url=geometryStore->GetHttpRoot()+"/";
	url+=path;
	// But even the path is incomplete, because it has no file extension. We want to send the
	// full URL of an actual file, so that even dumb fileservers or CDN's can respond with a download.
	url+=wrappedTexture->fileExtension();
	uint16_t urlLength = (uint16_t)url.length();
	if((size_t)urlLength != url.length())
		return avs::Result::Failed;
	//Push url length in 16 bits..
	put(urlLength);
	//Push name.
	put((uint8_t*)url.data(), urlLength);
	// Actual size is now known so update payload size
	putPayloadSize(uid);
	//Flag we have encoded the texture.
	geometryStreamingService->encodedResource(uid);
	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeTexturesBackend( std::vector<avs::uid> missingUIDs, bool)
{
	GeometryStore* geometryStore = &(GeometryStore::GetInstance());
	for (avs::uid uid : missingUIDs)
	{
		avs::Texture* texture;

		texture = geometryStore->getTexture(uid);
		if (texture)
		{
			if(texture->compressedData.size()==0)
			{
				TELEPORT_WARN_NOSPAM("Trying to send a zero-size texture {0}. Never do this!\n",texture->name);
				continue;
			}
			if (texture->compressedData.size()>INLINE_DATA_THRESHOLD_KB*1024&&geometryStore->IsHttpEnabled())
			{
				return encodeTexturePointer(uid);
			}
			if (texture->compression == avs::TextureCompression::UNCOMPRESSED)
			{
				TELEPORT_CERR << "Trying to send uncompressed texture " << texture->name << ". Never do this!\n";
				continue;
			}
 			if (texture->width == 0 || texture->height == 0)
			{
				TELEPORT_CERR << "Trying to send texture "<<texture->name<<" of zero size. Never do this!\n";
				continue;
			}
			//Place payload type onto the buffer.
			putPayloadType(avs::GeometryPayloadType::Texture,uid);
			size_t nameLength = texture->name.length();

			//Push name length.
			put(nameLength);
			//Push name.
			put((uint8_t*)texture->name.data(), nameLength);
			
			put(texture->compression);
			// The contents of compressedData should be identical to the texture's cache file, regardless of
			// format.
			put(texture->compressedData.data(), texture->compressedData.size());

			// Actual size is now known so update payload size
			putPayloadSize(uid);

			//Flag we have encoded the texture.
			geometryStreamingService->encodedResource(uid);
		}
		else
		{
			DEBUG_BREAK_ONCE("Missing texture");
			TELEPORT_WARN_NOSPAM("Trying to encode texture {0} but it is not there.",uid);
		}
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeFloatKeyframes(const std::vector<teleport::core::FloatKeyframe>& keyframes)
{
	put((uint16_t)keyframes.size());
	for (const teleport::core::FloatKeyframe& keyframe : keyframes)
	{
		put(keyframe.time);
		put(keyframe.value);
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeVector3Keyframes(const std::vector<teleport::core::Vector3Keyframe>& keyframes)
{
	put((uint16_t)keyframes.size());
	for (const teleport::core::Vector3Keyframe& keyframe : keyframes)
	{
		put(keyframe.time);
		put(keyframe.value);
	}

	return avs::Result::OK;
}

avs::Result GeometryEncoder::encodeVector4Keyframes(const std::vector<teleport::core::Vector4Keyframe>& keyframes)
{
	put((uint16_t)keyframes.size());
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
	if (buffer.size() + queuedBuffer.size() > serverSettings.geometryBufferCutoffSize)
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
