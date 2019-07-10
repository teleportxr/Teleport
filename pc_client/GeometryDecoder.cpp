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

avs::Result GeometryDecoder::decode(const void* buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface * target)
{
	// NO m_GALU Header or tail!
	/*std::vector<uint8_t> _buffer;
	_buffer.resize(bufferSizeInBytes);
	memcpy(_buffer.data(), (uint8_t*)buffer, bufferSizeInBytes);

	size_t offset = 0;
	#define Next8B get<uint64_t>(_buffer.data(), &offset)
	#define Next4B get<uint32_t>(_buffer.data(), &offset)


	DecodedGeometry dg = {};
	
	dg.primitiveArraysMapSize = Next8B;
	for (size_t i = 0; i < dg.primitiveArraysMapSize; i++)
	{
		avs::uid uid = Next8B;
		dg.primitiveArraysSizes[uid] = Next8B;
		for (size_t j = 0; j < dg.primitiveArraysSizes[uid]; j++)
		{
			size_t attributeCount = Next8B;
			avs::uid indices_accessor = Next8B;
			avs::uid material = Next8B;
			PrimitiveMode primitiveMode = (PrimitiveMode)Next4B;
			Attribute* attributes;
			
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

	dg.accessorsSize = Next8B;
	for (size_t j = 0; j < dg.accessorsSize; j++)
	{
		Accessor::DataType type = (Accessor::DataType)Next4B;
		Accessor::ComponentType componentType = (Accessor::ComponentType)Next4B;
		size_t count = Next8B;
		avs::uid bufferView = Next8B;
		size_t byteOffset = Next8B;

		dg.accessors.push_back({ type, componentType, count, bufferView, byteOffset });
	}

	dg.bufferViewsSize = Next8B;
	for (size_t j = 0; j < dg.bufferViewsSize; j++)
	{
		uid buffer = Next8B;
		size_t byteOffset = Next8B;
		size_t byteLength = Next8B;
		size_t byteStride = Next8B;

		dg.bufferViews.push_back({ buffer, byteOffset, byteLength, byteStride });
	}

	dg.buffersSize = Next8B;
	for (size_t j = 0; j < dg.buffersSize; j++)
	{
		dg.buffers.push_back({ 0, nullptr });
		dg.buffers.back().byteLength = Next8B;
		memcpy((void*)dg.buffers.back().data, (_buffer.data() + offset), dg.buffers.back().byteLength);
		offset += dg.buffers.back().byteLength;
	}*/
	//dg.~DecodedGeometry();
	//m_DecodedGeometries.push_back(dg);
		
	return avs::Result::OK;
}
