// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include <map>

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class GeometryDecoder final : public avs::GeometryDecoderBackendInterface
{
public:
	GeometryDecoder();
	~GeometryDecoder();
	// Inherited via GeometryDecoderBackendInterface
	virtual avs::Result decode(const void * buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target) override;

private:
	avs::Result decodeMesh(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeNode(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeMaterial(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeMaterialInstance(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeTexture(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeAnimation(avs::GeometryTargetBackendInterface*& target);

	//Use for the #define Next8B and #define Next4B macros
	std::vector<uint8_t> m_Buffer;
	size_t m_BufferSize= 0;
	size_t m_BufferOffset = 0;

#define Next8B get<uint64_t>(m_Buffer.data(), &m_BufferOffset)
#define Next4B get<uint32_t>(m_Buffer.data(), &m_BufferOffset)
#define NextB get<uint8_t>(m_Buffer.data(), &m_BufferOffset)
#define NextFloat get<float>(m_Buffer.data(), &m_BufferOffset)
#define NextChunk(T) get<T>(m_Buffer.data(), &m_BufferOffset)  

private:
	struct DecodedGeometry
	{
		std::map<avs::uid, std::vector<avs::PrimitiveArray>> primitiveArrays;
		std::map<avs::uid, avs::Accessor> accessors;
		std::map<avs::uid, avs::BufferView> bufferViews;
		std::map<avs::uid, avs::GeometryBuffer> buffers;
		std::map<avs::uid, std::vector<uint8_t>> bufferDatas;

		~DecodedGeometry()
		{
			for (auto& primitiveArray : primitiveArrays)
				primitiveArray.second.clear();
			primitiveArrays.clear();

			accessors.clear();
			bufferViews.clear();
			buffers.clear();

			for (auto& bufferData : bufferDatas)
				bufferData.second.clear();
			bufferDatas.clear();
		}
	};
};

