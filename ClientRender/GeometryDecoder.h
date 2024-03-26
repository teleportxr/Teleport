// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once
#include <libavstream/geometry/mesh_interface.hpp>
#include <libavstream/mesh.hpp>

#include "Platform/CrossPlatform/AxesStandard.h"
#include <map>
#include <parallel_hashmap/phmap.h>
#include <thread>

namespace teleport::core
{
	struct FloatKeyframe;
	struct Vector3Keyframe;
	struct Vector4Keyframe;
}

namespace draco
{
	class Mesh;
	class Scene;
}
namespace teleport
{
	namespace clientrender
	{
		class ResourceCreator;
		enum class GeometryFileFormat
		{
			TELEPORT_NATIVE,
			GLTF_TEXT,
			GLTF_BINARY
		};
		/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
		 */
		class GeometryDecoder final : public avs::GeometryDecoderBackendInterface
		{
		private:
			struct DecodedGeometry;

		private:
			struct GeometryDecodeData
			{
				avs::uid server_or_cache_uid = 0;
				std::string filename_or_url;
				uint16_t version = 0;
				std::vector<uint8_t> data = {};
				size_t bytesRemaining() const
				{
					if(offset<data.size())
						return data.size()-offset;
					else
						return 0;
				}
				size_t offset = 0;
				avs::GeometryPayloadType type = avs::GeometryPayloadType::Invalid;
				GeometryFileFormat geometryFileFormat = GeometryFileFormat::TELEPORT_NATIVE;
				clientrender::ResourceCreator *target = nullptr;
				bool saveToDisk = false;
				avs::uid uid = 0;
				platform::crossplatform::AxesStandard sourceAxesStandard = platform::crossplatform::AxesStandard::Engineering;
				GeometryDecodeData(avs::uid server_cache_uid, std::string filename_url, const void *ptr, size_t size, avs::GeometryPayloadType type_, GeometryFileFormat f, clientrender::ResourceCreator *target_, bool saveToDisk_, avs::uid u, platform::crossplatform::AxesStandard ax)
					: server_or_cache_uid(server_cache_uid), filename_or_url(filename_url), data(size), type(type_), geometryFileFormat(f), target(target_), saveToDisk(saveToDisk_), uid(u), sourceAxesStandard(ax)
				{
					memcpy(data.data(), ptr, size);
				}
			};

		public:
			GeometryDecoder();
			~GeometryDecoder();

			void setCacheFolder(const std::string &f);

			//! Inherited via GeometryDecoderBackendInterface.
			virtual avs::Result decode(avs::uid server_uid, const void *buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface *target, avs::uid resource_uid) override;
			//! Treat the file as buffer input and decode. Note: uid must be supplied, as against in decode, where it is read from the first 8 bytes.
			avs::Result decodeFromFile(avs::uid server_uid, const std::string &filename, avs::GeometryPayloadType type, clientrender::ResourceCreator *intf, avs::uid uid, platform::crossplatform::AxesStandard sourceAxesStandard = platform::crossplatform::AxesStandard::Engineering);
			//! Put out an HTTPS request for the specified data.
			avs::Result decodeFromWeb(avs::uid server_uid, const std::string &uri, avs::GeometryPayloadType type, clientrender::ResourceCreator *intf, avs::uid uid, platform::crossplatform::AxesStandard sourceAxesStandard = platform::crossplatform::AxesStandard::Engineering);
			//! Callback that receives HTTPS data.
			avs::Result receiveFromWeb(avs::uid server_uid, std::string uri, const uint8_t *buffer, size_t bufferSize, avs::GeometryPayloadType type, clientrender::ResourceCreator *target, avs::uid resource_uid, platform::crossplatform::AxesStandard sourceAxesStandard);
			//! Treat the buffer as input and decode as we would a file.
			avs::Result decodeFromBuffer(avs::uid server_uid, const uint8_t *buffer, size_t bufferSize, const std::string &filename, avs::GeometryPayloadType type, clientrender::ResourceCreator *intf, avs::uid uid, platform::crossplatform::AxesStandard sourceAxesStandard = platform::crossplatform::AxesStandard::Engineering);
			inline void WaitFromDecodeThread()
			{
				// Ugly thread spin lock
				while (!decodeData.empty())
				{
				}
			}

		private:
			void decodeAsync();
			avs::Result decodeInternal(GeometryDecodeData &geometryDecodeData);
			avs::Result DecodeGltf(const GeometryDecodeData &geometryDecodeData);
			avs::Result DracoMeshToPrimitiveArray(avs::uid primitiveArrayUid, DecodedGeometry &dg, const draco::Mesh &dracoMesh, const avs::CompressedSubMesh &compressedSubMesh, platform::crossplatform::AxesStandard axesStandard);
			avs::Result DracoMeshToDecodedGeometry(avs::uid primitiveArrayUid, DecodedGeometry &dg, draco::Mesh &dracoMesh, platform::crossplatform::AxesStandard axesStandard);
			avs::Result DracoMeshToDecodedGeometry(avs::uid primitiveArrayUid, DecodedGeometry &dg, const avs::CompressedMesh &compressedMesh, platform::crossplatform::AxesStandard axesStandard);
			avs::Result DecodeDracoScene(clientrender::ResourceCreator *target, std::string filename_url, avs::uid server_or_cache_uid, avs::uid primitiveArrayUid, draco::Scene &dracoScene, platform::crossplatform::AxesStandard axesStandard);
			avs::Result CreateFromDecodedGeometry(clientrender::ResourceCreator *target, DecodedGeometry &dg, const std::string &name);

			avs::Result decodeMesh(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeMaterial(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeMaterialInstance(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeTexture(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeAnimation(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeNode(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeSkeleton(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeFontAtlas(GeometryDecodeData &geometryDecodeData);
			avs::Result decodeTextCanvas(GeometryDecodeData &geometryDecodeData);

			avs::Result decodeFloatKeyframes(GeometryDecodeData &geometryDecodeData, std::vector<teleport::core::FloatKeyframe> &keyframes);
			avs::Result decodeVector3Keyframes(GeometryDecodeData &geometryDecodeData, std::vector<teleport::core::Vector3Keyframe> &keyframes);
			avs::Result decodeVector4Keyframes(GeometryDecodeData &geometryDecodeData, std::vector<teleport::core::Vector4Keyframe> &keyframes);

			void saveBuffer(GeometryDecodeData &geometryDecodeData, const std::string &name);

			// Use for data extracted from compressed objects.
			std::vector<std::vector<uint8_t>> m_DecompressedBuffers;
			size_t m_DecompressedBufferIndex = 0;

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
				vec4 transform; // to be applied on creation.
			};
			struct DecodedGeometry
			{
				avs::uid server_or_cache_uid = 0;
				platform::crossplatform::AxesStandard axesStandard = platform::crossplatform::AxesStandard::Engineering;
				// Optional, for creating local subgraphs.
				phmap::flat_hash_map<avs::uid, avs::Node> nodes;
				phmap::flat_hash_map<avs::uid, avs::Skeleton> skeletons;
				phmap::flat_hash_map<avs::uid, std::vector<PrimitiveArray>> primitiveArrays;
				phmap::flat_hash_map<uint64_t, avs::Accessor> accessors;
				phmap::flat_hash_map<uint64_t, avs::BufferView> bufferViews;
				phmap::flat_hash_map<uint64_t, avs::GeometryBuffer> buffers;
				phmap::flat_hash_map<avs::uid, avs::Material> internalMaterials;
				std::vector<mat4> inverseBindMatrices;
				bool clockwiseFaces = true;
				// For internal numbering of accessors etc.
				uint64_t next_id = 0;
				void clear()
				{
					primitiveArrays.clear();
					accessors.clear();
					bufferViews.clear();
					buffers.clear();
					internalMaterials.clear();
					next_id = 0;
					clockwiseFaces = true;
				}
				~DecodedGeometry()
				{
					for (auto &primitiveArray : primitiveArrays)
					{
						for (auto &primitive : primitiveArray.second)
						{
							primitive.attributes.clear();
						}
						primitiveArray.second.clear();
					}
					primitiveArrays.clear();
					accessors.clear();
					bufferViews.clear();
					buffers.clear();
					internalMaterials.clear();
				}
			};
		};
	}
}