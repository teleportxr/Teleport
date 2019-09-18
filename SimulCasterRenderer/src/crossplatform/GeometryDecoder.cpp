#include "GeometryDecoder.h"
#include <iostream>
#include <Common.h>


using namespace avs;

GeometryDecoder::GeometryDecoder()
{
}


GeometryDecoder::~GeometryDecoder()
{
}

template<typename T> T get(const uint8_t* data, size_t* offset)
{
	T* t = (T*)(data + (*offset));
	*offset += sizeof(T);
	return *t;
}
template<typename T> size_t get(const T* &data)
{
	const T& t = *data;
	data++;
	return t;
}

template<typename T> void get(T* target,const uint8_t *data, size_t count)
{
	memcpy(target, data, count*sizeof(T));
}

template<typename T> void copy(T* target, const uint8_t *data, size_t &dataOffset, size_t count)
{
	memcpy(target, data + dataOffset, count * sizeof(T));
	dataOffset += count;
}

avs::Result GeometryDecoder::decode(const void* buffer, size_t bufferSizeInBytes, GeometryPayloadType type, GeometryTargetBackendInterface* target)
{
	// No m_GALU on the header or tail on the incoming buffer!
	m_BufferSize = bufferSizeInBytes;
	m_BufferOffset = 0;
	m_Buffer.clear();
	m_Buffer.resize(m_BufferSize);
	memcpy(m_Buffer.data(), (uint8_t*)buffer, m_BufferSize);

	switch (type)
	{
	case GeometryPayloadType::Mesh:
	{
		return decodeMesh(target);
	}
	case GeometryPayloadType::Material:
	{
		return decodeMaterial(target);
	}
	case GeometryPayloadType::MaterialInstance:
	{
		return decodeMaterialInstance(target);
	}
	case GeometryPayloadType::Texture:
	{
		return decodeTexture(target);
	}
	case GeometryPayloadType::Animation:
	{
		return decodeAnimation(target);
	}
	case GeometryPayloadType::Node:
	{
		return decodeNode(target);
	}

	default:
	{ 
		return avs::Result::GeometryDecoder_InvalidPayload;
	}
	};
}

avs::Result GeometryDecoder::decodeMesh(GeometryTargetBackendInterface*& target)
{
	//Parse buffer and fill struct DecodedGeometry
	DecodedGeometry dg = {};
	avs::uid uid;

	size_t meshCount = Next8B;
	for (size_t i = 0; i < meshCount; i++)
	{
		uid = Next8B; 
		size_t primitiveArraysSize = Next8B;
		for (size_t j = 0; j < primitiveArraysSize; j++)
		{
			size_t attributeCount = Next8B;
			avs::uid indices_accessor = Next8B;
			avs::uid material = Next8B;
			PrimitiveMode primitiveMode = (PrimitiveMode)Next4B;

			std::vector<Attribute> attributes;
			attributes.reserve(attributeCount);
			for (size_t k = 0; k < attributeCount; k++)
			{
				AttributeSemantic semantic = (AttributeSemantic)Next8B;
				avs::uid accessor = Next8B;
				attributes.push_back({ semantic, accessor });
			}

			dg.primitiveArrays[uid].push_back({ attributeCount, attributes, indices_accessor, material, primitiveMode });
		}
	}

	bool isIndexAccessor = true;
//	size_t primitiveArrayIndex = 0;
//	size_t k = 0;
	size_t accessorsSize = Next8B;
	for (size_t j = 0; j < accessorsSize; j++)
	{
		avs::uid acc_uid= Next8B;
		Accessor::DataType type = (Accessor::DataType)Next4B;
		Accessor::ComponentType componentType = (Accessor::ComponentType)Next4B;
		size_t count = Next8B;
		avs::uid bufferView = Next8B;
		size_t byteOffset = Next8B;

		if (isIndexAccessor) //For Indices Only
		{
			dg.accessors[acc_uid] = { type, componentType, count, bufferView, byteOffset };
			isIndexAccessor = false;
		}
		else
		{
			dg.accessors[acc_uid] = { type, componentType, count, bufferView, byteOffset };
		}
		
	}
	size_t bufferViewsSize = Next8B;
	for (size_t j = 0; j < bufferViewsSize; j++)
	{
		avs::uid bv_uid = Next8B;
		avs::uid buffer = Next8B;
		size_t byteOffset = Next8B;
		size_t byteLength = Next8B;
		size_t byteStride = Next8B;
		
		dg.bufferViews[bv_uid] = { buffer, byteOffset, byteLength, byteStride };
	}

	size_t buffersSize = Next8B;
	for (size_t j = 0; j < buffersSize; j++)
	{
		avs::uid key = Next8B;
		dg.buffers[key]= { 0, nullptr };
		dg.buffers[key].byteLength = Next8B;
		if(m_BufferSize < m_BufferOffset + dg.buffers[key].byteLength)
		{
			return avs::Result::GeometryDecoder_InvalidBufferSize;
		}

		dg.bufferDatas[key].push_back({});
		dg.bufferDatas[key].resize(dg.buffers[key].byteLength);

		memcpy((void*)dg.bufferDatas[key].data(), (m_Buffer.data() + m_BufferOffset), dg.buffers[key].byteLength);
		dg.buffers[key].data = dg.bufferDatas[key].data();

		m_BufferOffset += dg.buffers[key].byteLength;
	}

	//Push data to GeometryTargetBackendInterface
	for (std::map<avs::uid, std::vector<PrimitiveArray2>>::iterator it = dg.primitiveArrays.begin();
		it != dg.primitiveArrays.end(); it++)
	{
		size_t index = 0;
		MeshCreate meshCreate;
		meshCreate.mesh_uid = it->first;
		meshCreate.m_NumElements = it->second.size();
		// Primitive array elements in each mesh.
		for (const auto& primitive : it->second)
		{
			MeshElementCreate& meshElementCreate = meshCreate.m_MeshElementCreate[index];
			meshElementCreate.vb_uid = primitive.attributes[0].accessor;
			size_t vertexCount = 0;
			for (size_t i = 0; i < primitive.attributeCount; i++)
			{
				//Vertices
				const Attribute& attrib			= primitive.attributes[i];
				const Accessor& accessor		= dg.accessors[attrib.accessor];
				const BufferView& bufferView	= dg.bufferViews[accessor.bufferView];
				const GeometryBuffer& buffer	= dg.buffers[bufferView.buffer];
				const uint8_t* data				= buffer.data+bufferView.byteOffset;
				switch (attrib.semantic)
				{
				case AttributeSemantic::POSITION:
					meshElementCreate.m_VertexCount = vertexCount = accessor.count;
					meshElementCreate.m_Vertices = (const avs::vec3*)(data);
					continue;
				case AttributeSemantic::TANGENTNORMALXZ:
				{
					size_t tnSize = 0;
					tnSize = avs::GetComponentSize(accessor.componentType) * avs::GetDataTypeSize(accessor.type);
					meshElementCreate.m_TangentNormalSize = tnSize;
					meshElementCreate.m_TangentNormals= (const uint8_t*)buffer.data;
				}
					continue;
				case AttributeSemantic::NORMAL:
					meshElementCreate.m_Normals = (const avs::vec3*)(data);
					assert(accessor.count / 8 == vertexCount);
					continue;
				case AttributeSemantic::TANGENT:
					meshElementCreate.m_Tangents = (const avs::vec4*)(data);
					assert(accessor.count / 8 == vertexCount);
					continue;
				case AttributeSemantic::TEXCOORD_0:
					meshElementCreate.m_UV0s = (const avs::vec2*)(data);
					assert(accessor.count == vertexCount);
					continue;
				case AttributeSemantic::TEXCOORD_1:
					meshElementCreate.m_UV1s = (const avs::vec2*)(data);
					assert(accessor.count == vertexCount);
					continue;;
				case AttributeSemantic::COLOR_0:
					//target->ensureColors(mesh_uid, 0, (int)accessor.count, (const avs::vec4*)buffer.data);
					continue;
				case AttributeSemantic::JOINTS_0:
					//target->ensureJoints(it->first, 0, (int)accessor.count, (const avs::vec4*)buffer.data);
					continue;
				case AttributeSemantic::WEIGHTS_0:
					//target->ensureWeights(it->first, 0, (int)accessor.count, (const avs::vec4*)buffer.data);
					continue;
				default:
				    SCR_CERR("Unknown attribute semantic: " << (uint32_t)attrib.semantic);
				    continue;
				}
			}

			//Indices
			const Accessor& indicesAccessor = dg.accessors[primitive.indices_accessor];
			const BufferView& indicesBufferView = dg.bufferViews[indicesAccessor.bufferView];
			const GeometryBuffer& indicesBuffer = dg.buffers[indicesBufferView.buffer];
			size_t componentSize = avs::GetComponentSize(indicesAccessor.componentType);
			meshElementCreate.ib_uid = primitive.indices_accessor;
			meshElementCreate.m_Indices = (indicesBuffer.data+indicesBufferView.byteOffset+ indicesAccessor.byteOffset);
			meshElementCreate.m_IndexSize = componentSize;
			meshElementCreate.m_IndexCount = indicesAccessor.count;
			meshElementCreate.m_ElementIndex = index;
			index++;
		}
		avs::Result result = target->Assemble(&meshCreate);
		if (result != avs::Result::OK)
			return result;
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeMaterial(GeometryTargetBackendInterface*& target)
{
	size_t materialAmount = Next8B;

	for(size_t i = 0; i < materialAmount; i++)
	{
		Material material;
		
		avs::uid mat_uid = Next8B;

		size_t nameLength = Next8B;

		material.name.resize(nameLength);
		copy<char>(material.name.data(), m_Buffer.data(), m_BufferOffset, nameLength);
		
		material.pbrMetallicRoughness.baseColorTexture.index = Next8B;
		material.pbrMetallicRoughness.baseColorTexture.texCoord = Next8B;
		material.pbrMetallicRoughness.baseColorTexture.tiling.x = NextFloat;
		material.pbrMetallicRoughness.baseColorTexture.tiling.y = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.x = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.y = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.z = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.w = NextFloat;

		material.pbrMetallicRoughness.metallicRoughnessTexture.index = Next8B;
		material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = Next8B;
		material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.x = NextFloat;
		material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.y = NextFloat;
		material.pbrMetallicRoughness.metallicFactor = NextFloat;
		material.pbrMetallicRoughness.roughnessFactor = NextFloat;

		material.normalTexture.index = Next8B;
		material.normalTexture.texCoord = Next8B;
		material.normalTexture.tiling.x = NextFloat;
		material.normalTexture.tiling.y = NextFloat;
		material.normalTexture.scale = NextFloat;

		material.occlusionTexture.index = Next8B;
		material.occlusionTexture.texCoord = Next8B;
		material.occlusionTexture.tiling.x = NextFloat;
		material.occlusionTexture.tiling.y = NextFloat;
		material.occlusionTexture.strength = NextFloat;

		material.emissiveTexture.index = Next8B;
		material.emissiveTexture.texCoord = Next8B;
		material.emissiveTexture.tiling.x = NextFloat;
		material.emissiveTexture.tiling.y = NextFloat;
		material.emissiveFactor.x = NextFloat;
		material.emissiveFactor.y = NextFloat;
		material.emissiveFactor.z = NextFloat;

		size_t extensionAmount = Next8B;
		for(size_t i = 0; i < extensionAmount; i++)
		{
			std::unique_ptr<MaterialExtension> newExtension;
			MaterialExtensionIdentifier id = static_cast<MaterialExtensionIdentifier>(Next4B);

			switch(id)
			{
				case MaterialExtensionIdentifier::SIMPLE_GRASS_WIND:
					newExtension = std::make_unique<SimpleGrassWindExtension>();
					newExtension->deserialise(m_Buffer, m_BufferOffset);
					break;
			}

			material.extensions[id] = std::move(newExtension);
		}

		target->passMaterial(mat_uid, material);
	}
	
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeMaterialInstance(GeometryTargetBackendInterface*& target)
{
	return avs::Result::GeometryDecoder_Incomplete;
}

Result GeometryDecoder::decodeTexture(GeometryTargetBackendInterface *& target)
{
	size_t textureAmount = Next8B;
	for(size_t i = 0; i < textureAmount; i++)
	{
		Texture texture;
		uid texture_uid = Next8B;

		size_t nameLength = Next8B;
		texture.name.resize(nameLength);
		copy<char>(texture.name.data(), m_Buffer.data(), m_BufferOffset, nameLength);

		texture.width = Next4B;
		texture.height = Next4B;
		texture.depth = Next4B;
		texture.bytesPerPixel = Next4B;
		texture.arrayCount = Next4B;
		texture.mipCount = Next4B;
		texture.format = static_cast<avs::TextureFormat>(Next4B);

		texture.dataSize = Next4B;
		texture.data = new unsigned char[texture.dataSize];
		copy<unsigned char>(texture.data, m_Buffer.data(), m_BufferOffset, texture.dataSize);

		texture.sampler_uid = Next8B;

		target->passTexture(texture_uid, texture);
	}

	return Result::OK;
}

avs::Result GeometryDecoder::decodeAnimation(GeometryTargetBackendInterface*& target)
{
	return avs::Result::GeometryDecoder_Incomplete;
}

avs::Result GeometryDecoder::decodeNode(avs::GeometryTargetBackendInterface*& target)
{
	uint32_t nodeCount = Next8B;
	for (uint32_t i = 0; i < nodeCount; ++i)
	{
		avs::uid uid = Next8B;

		avs::DataNode node;

		node.transform = NextChunk(avs::Transform);

		node.data_uid = Next8B;

		node.data_type = static_cast<NodeDataType>(NextB);
		
		uint32_t materialCount = Next8B;
		for (uint32_t j = 0; j < materialCount; ++j)
		{
			node.materials.push_back(Next8B);
		}

		uint32_t childCount = Next8B;
		for (uint32_t j = 0; j < childCount; ++j)
		{
			node.childrenUids.push_back(Next8B);
		}
		target->passNode(uid, node);
	}
	return avs::Result::OK;
}
