// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include <map>
#include <thread>

namespace avs
{
struct FloatKeyframe;
struct Vector3Keyframe;
struct Vector4Keyframe;
}

namespace draco
{
	class Mesh;
}

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class GeometryDecoder final : public avs::GeometryDecoderBackendInterface
{
	//Forward Declarations
private:
	struct DecodedGeometry;

	//enums/struct
private:
	struct GeometryDecodeData
	{
		std::vector<uint8_t>					data		= {};
		size_t									offset		= 0;
		avs::GeometryPayloadType				type		= avs::GeometryPayloadType::Invalid;
		avs::GeometryTargetBackendInterface*	target		= nullptr;
		bool									saveToDisk	= false;

		GeometryDecodeData(const void* ptr, size_t size, avs::GeometryPayloadType type_, avs::GeometryTargetBackendInterface* target_, bool saveToDisk_)
			: data(size), type(type_), target(target_), saveToDisk(saveToDisk_) 
		{
			memcpy(data.data(), ptr, size);
		}
	};

public:
	GeometryDecoder();
	~GeometryDecoder();

	void setCacheFolder(const std::string &f);

	//! Inherited via GeometryDecoderBackendInterface
	virtual avs::Result decode(const void * buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target) override;
	//! Treat the file as buffer input and decode.
	avs::Result decodeFromFile(const std::string &filename,avs::GeometryPayloadType type,avs::GeometryTargetBackendInterface *intf);

	inline void FlushDecodeThread()
	{
		//Ugly thread spin lock
		while (!decodeData.empty()) {}
	}

private:
	void decodeAsync();
	avs::Result decodeInternal(GeometryDecodeData& geometryDecodeData);
	
	avs::Result DracoMeshToDecodedGeometry(avs::uid primitiveArrayUid, DecodedGeometry& dg, const avs::CompressedMesh& compressedMesh);
	avs::Result CreateMeshesFromDecodedGeometry(avs::GeometryTargetBackendInterface* target, DecodedGeometry& dg, const std::string& name);

	avs::Result decodeMesh(GeometryDecodeData& geometryDecodeData);
	avs::Result decodeMaterial(GeometryDecodeData& geometryDecodeData);
	avs::Result decodeMaterialInstance(GeometryDecodeData& geometryDecodeData);
	avs::Result decodeTexture(GeometryDecodeData& geometryDecodeData);
	avs::Result decodeAnimation(GeometryDecodeData& geometryDecodeData);
	avs::Result decodeNode(GeometryDecodeData& geometryDecodeData);
	avs::Result decodeSkin(GeometryDecodeData& geometryDecodeData);

	avs::Result decodeFloatKeyframes(GeometryDecodeData& geometryDecodeData, std::vector<avs::FloatKeyframe>& keyframes);
	avs::Result decodeVector3Keyframes(GeometryDecodeData& geometryDecodeData, std::vector<avs::Vector3Keyframe>& keyframes);
	avs::Result decodeVector4Keyframes(GeometryDecodeData& geometryDecodeData, std::vector<avs::Vector4Keyframe>& keyframes);
	
	void saveBuffer(GeometryDecodeData& geometryDecodeData, const std::string& name);

	// Use for data extracted from compressed objects.
	std::vector<std::vector<uint8_t>> m_DecompressedBuffers;
	size_t m_DecompressedBufferIndex=0;

private:
	std::thread decodeThread;
	bool decodeThreadActive;
	avs::ThreadSafeQueue<GeometryDecodeData> decodeData;

private:
	std::string cacheFolder;
	struct PrimitiveArray
	{
		size_t attributeCount;
		std::vector<avs::Attribute> attributes;
		avs::uid indices_accessor;
		avs::uid material;
		avs::PrimitiveMode primitiveMode;
	};
	struct DecodedGeometry
	{
		std::unordered_map<avs::uid, std::vector<PrimitiveArray>> primitiveArrays;
		std::unordered_map<avs::uid, avs::Accessor> accessors;
		std::unordered_map<avs::uid, avs::BufferView> bufferViews;
		std::unordered_map<avs::uid, avs::GeometryBuffer> buffers;
		void clear()
		{
			primitiveArrays.clear();
			accessors.clear();
			bufferViews.clear();
			buffers.clear();
		}
		~DecodedGeometry()
		{
			for (auto& primitiveArray : primitiveArrays)
			{
				for (auto& primitive : primitiveArray.second)
				{
					primitive.attributes.clear();
				}
				primitiveArray.second.clear();
			}
			primitiveArrays.clear();

			accessors.clear();
			bufferViews.clear();
			buffers.clear();
		}
	};
};