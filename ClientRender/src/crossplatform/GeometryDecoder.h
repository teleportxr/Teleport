// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include <map>

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
public:
	GeometryDecoder();
	~GeometryDecoder();
	//! Treat the file as buffer input and decode.
	avs::Result decodeFromFile(const std::string &filename,avs::GeometryTargetBackendInterface *intf);
	// Inherited via GeometryDecoderBackendInterface
	virtual avs::Result decode(const void * buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target) override;
	
	void saveBuffer(const std::string &name) const;
private:
	avs::Result decode(avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target);
	
	avs::Result decodeMesh(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeMaterial(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeMaterialInstance(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeTexture(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeAnimation(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeNode(avs::GeometryTargetBackendInterface*& target);
	avs::Result decodeSkin(avs::GeometryTargetBackendInterface*& target);

	avs::Result decodeFloatKeyframes(std::vector<avs::FloatKeyframe>& keyframes);
	avs::Result decodeVector3Keyframes(std::vector<avs::Vector3Keyframe>& keyframes);
	avs::Result decodeVector4Keyframes(std::vector<avs::Vector4Keyframe>& keyframes);

	//Use for the #define Next8B and #define Next4B macros
	std::vector<uint8_t> m_Buffer;
	size_t m_BufferSize= 0;
	size_t m_BufferOffset = 0;
	// Use for data extracted from compressed objects.
	std::vector<std::vector<uint8_t>> m_DecompressedBuffers;
	size_t m_DecompressedBufferIndex=0;

private:
	struct PrimitiveArray2
	{
		size_t attributeCount;
		std::vector<avs::Attribute> attributes;
		avs::uid indices_accessor;
		avs::uid material;
		avs::PrimitiveMode primitiveMode;
	};
	struct DecodedGeometry
	{
		std::unordered_map<avs::uid, std::vector<PrimitiveArray2>> primitiveArrays;
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
	DecodedGeometry dg = {};
	avs::Result DracoMeshToDecodedGeometry(avs::uid primitiveArrayUid,DecodedGeometry& dg, const avs::CompressedMesh &compressedMesh);
};

