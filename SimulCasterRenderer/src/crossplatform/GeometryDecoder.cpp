#include "GeometryDecoder.h"
#include <iostream>

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
			Attribute attributes[(size_t)AttributeSemantic::COUNT];

			std::vector<Attribute> _attrib;
			_attrib.reserve(attributeCount);
			for (size_t k = 0; k < attributeCount; k++)
			{
				AttributeSemantic semantic = (AttributeSemantic)Next4B;
				Next4B;
				avs::uid accessor = Next8B;
				_attrib.push_back({ semantic, accessor });
			}
			memcpy(attributes, _attrib.data(), attributeCount * sizeof(Attribute));

			dg.primitiveArrays[uid].push_back({ attributeCount, attributes, indices_accessor, material, primitiveMode });
		}
	}

	bool isIndexAccessor = true;
	size_t primitiveArrayIndex = 0;
	size_t k = 0;
	size_t accessorsSize = Next8B;
	for (size_t j = 0; j < accessorsSize; j++)
	{
		Accessor::DataType type = (Accessor::DataType)Next4B;
		Accessor::ComponentType componentType = (Accessor::ComponentType)Next4B;
		size_t count = Next8B;
		avs::uid bufferView = Next8B;
		size_t byteOffset = Next8B;

		size_t primitiveArrayAttributeCount = dg.primitiveArrays[uid][primitiveArrayIndex].attributeCount;
		if (isIndexAccessor) //For Indices Only
		{
			dg.accessors[dg.primitiveArrays[uid][primitiveArrayIndex].indices_accessor] = { type, componentType, count, bufferView, byteOffset };
			isIndexAccessor = false;
		}
		else
		{
			dg.accessors[dg.primitiveArrays[uid][primitiveArrayIndex].attributes[k].accessor] = { type, componentType, count, bufferView, byteOffset };
			k++;
			if (k < primitiveArrayAttributeCount == false)
			{
				
				isIndexAccessor = true;
				primitiveArrayIndex++;
				k = 0;
			}
		}
		
	}

	std::map<avs::uid, avs::Accessor>::reverse_iterator rit_accessor = dg.accessors.rbegin();
	std::map<avs::uid, avs::Accessor>::iterator it_accessor = dg.accessors.begin();
	size_t bufferViewsSize = Next8B;
	for (size_t j = 0; j < bufferViewsSize; j++)
	{
		avs::uid buffer = Next8B;
		size_t byteOffset = Next8B;
		size_t byteLength = Next8B;
		size_t byteStride = Next8B;

		avs::uid key = 0;
		if (j == 0)
			key = dg.accessors[rit_accessor->first].bufferView;
		else
		{
			key = dg.accessors[it_accessor->first].bufferView;
			it_accessor++;
		}
		
		dg.bufferViews[key] = { buffer, byteOffset, byteLength, byteStride };
	}

	std::map<avs::uid, avs::BufferView>::reverse_iterator rit_bufferView = dg.bufferViews.rbegin();
	std::map<avs::uid, avs::BufferView>::iterator it_bufferView = dg.bufferViews.begin();
	size_t buffersSize = Next8B;
	for (size_t j = 0; j < buffersSize; j++)
	{
		avs::uid key = 0;
		if (j == 0)
			key = dg.bufferViews[rit_bufferView->first].buffer;
		else
		{
			key = dg.bufferViews[it_bufferView->first].buffer;
			it_bufferView++;
		}
		
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
	for (std::map<avs::uid, std::vector<avs::PrimitiveArray>>::iterator it = dg.primitiveArrays.begin();
		it != dg.primitiveArrays.end(); it++)
	{
		for (auto& primitive : it->second)
		{
			for (size_t i = 0; i < primitive.attributeCount; i++)
			{
				//Vertices
				Attribute attrib = primitive.attributes[i];
				const Accessor &accessor = dg.accessors[attrib.accessor];
				switch (attrib.semantic)
				{
				case AttributeSemantic::POSITION:
					target->ensureVertices(it->first, 0, (int)accessor.count, (const avs::vec3*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				case AttributeSemantic::TANGENTNORMALXZ:
				{
					size_t tnSize = 0;
					if (accessor.type == avs::Accessor::DataType::VEC2)
						tnSize = 8;
					else  if (accessor.type == avs::Accessor::DataType::VEC4)
						tnSize = 16;
					target->ensureTangentNormals(it->first, 0, (int)accessor.count, tnSize, (const uint8_t*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
				}
					continue;
				case AttributeSemantic::NORMAL:
					target->ensureNormals(it->first, 0, (int)accessor.count, (const avs::vec3*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				case AttributeSemantic::TANGENT:
					target->ensureTangents(it->first, 0, (int)accessor.count, (const avs::vec4*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				case AttributeSemantic::TEXCOORD_0:
					target->ensureTexCoord0(it->first, 0, (int)accessor.count, (const avs::vec2*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				case AttributeSemantic::TEXCOORD_1:
					target->ensureTexCoord1(it->first, 0, (int)accessor.count, (const avs::vec2*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				case AttributeSemantic::COLOR_0:
					target->ensureColors(it->first, 0, (int)accessor.count, (const avs::vec4*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				case AttributeSemantic::JOINTS_0:
					target->ensureJoints(it->first, 0, (int)accessor.count, (const avs::vec4*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				case AttributeSemantic::WEIGHTS_0:
					target->ensureWeights(it->first, 0, (int)accessor.count, (const avs::vec4*)dg.buffers[dg.bufferViews[accessor.bufferView].buffer].data);
					continue;
				default:
				    std::cerr<<"Unknown attribute semantic\n";
				    continue;
				}
			}

			//Indices
			size_t componentSize = avs::GetComponentSize(dg.accessors[primitive.indices_accessor].componentType);
			target->ensureIndices(it->first, (int)(dg.accessors[primitive.indices_accessor].byteOffset / componentSize), (int)dg.accessors[primitive.indices_accessor].count, (int)componentSize,dg.buffers[dg.bufferViews[dg.accessors[primitive.indices_accessor].bufferView].buffer].data);
			avs::Result result = target->Assemble();
			if (result != avs::Result::OK)
				return result;
		}
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
		material.pbrMetallicRoughness.baseColorFactor.x = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.y = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.z = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.w = NextFloat;

		material.pbrMetallicRoughness.metallicRoughnessTexture.index = Next8B;
		material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = Next8B;
		material.pbrMetallicRoughness.metallicFactor = NextFloat;
		material.pbrMetallicRoughness.roughnessFactor = NextFloat;

		material.normalTexture.index = Next8B;
		material.normalTexture.texCoord = Next8B;
		material.normalTexture.scale = NextFloat;

		material.occlusionTexture.index = Next8B;
		material.occlusionTexture.texCoord = Next8B;
		material.occlusionTexture.strength = NextFloat;

		material.emissiveTexture.index = Next8B;
		material.emissiveTexture.texCoord = Next8B;
		material.emissiveFactor.x = NextFloat;
		material.emissiveFactor.y = NextFloat;
		material.emissiveFactor.z = NextFloat;

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

		size_t textureSize = Next8B;

		texture.data = new unsigned char[textureSize];
		copy<unsigned char>(texture.data, m_Buffer.data(), m_BufferOffset, textureSize);

		texture.sampler_uid = Next8B;

		target->passTexture(texture_uid, texture);
	}

	return Result::OK;
}

avs::Result GeometryDecoder::decodeAnimation(GeometryTargetBackendInterface*& target)
{
	return avs::Result::GeometryDecoder_Incomplete;
}
