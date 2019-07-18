#include "GeometryDecoder.h"

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

avs::Result GeometryDecoder::decode(const void* buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target)
{
	// NO m_GALU Header or tail!
	std::vector<uint8_t> _buffer;
	_buffer.resize(bufferSizeInBytes);
	memcpy(_buffer.data(), (uint8_t*)buffer, bufferSizeInBytes);

	size_t offset = 0;
	#define Next8B get<uint64_t>(_buffer.data(), &offset)
	#define Next4B get<uint32_t>(_buffer.data(), &offset)

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
		assert(bufferSizeInBytes >= offset + dg.buffers[key].byteLength);

		dg.bufferDatas[key].push_back({});
		dg.bufferDatas[key].resize(dg.buffers[key].byteLength);

		memcpy((void*)dg.bufferDatas[key].data(), (_buffer.data() + offset), dg.buffers[key].byteLength);
		dg.buffers[key].data = dg.bufferDatas[key].data();

		offset += dg.buffers[key].byteLength;
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
				switch (attrib.semantic)
				{
				case AttributeSemantic::POSITION:
					target->ensureVertices(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec3*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				case AttributeSemantic::NORMAL:
					target->ensureNormals(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec3*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				case AttributeSemantic::TANGENT:
					target->ensureTangents(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec4*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				case AttributeSemantic::TEXCOORD_0:
					target->ensureTexCoord0(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec2*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				case AttributeSemantic::TEXCOORD_1:
					target->ensureTexCoord1(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec2*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				case AttributeSemantic::COLOR_0:
					target->ensureColors(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec4*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				case AttributeSemantic::JOINTS_0:
					target->ensureJoints(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec4*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				case AttributeSemantic::WEIGHTS_0:
					target->ensureWeights(it->first, 0, (int)dg.accessors[attrib.accessor].count, (const avs::vec4*)dg.buffers[dg.bufferViews[dg.accessors[attrib.accessor].bufferView].buffer].data);
					continue;
				}
			}

			//Indices
			target->ensureIndices(it->first, 0, (int)dg.accessors[primitive.indices_accessor].count, (const unsigned int*)dg.buffers[dg.bufferViews[dg.accessors[primitive.indices_accessor].bufferView].buffer].data);
		}
	}

	return avs::Result::OK;
}
