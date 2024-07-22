// (C) Copyright 2018-2024 Simul Software Ltd
#pragma once
#include <libavstream/geometry/mesh_interface.hpp>
#include <libavstream/mesh.hpp>

#include "Platform/CrossPlatform/AxesStandard.h"
#include <map>
#include <parallel_hashmap/phmap.h>
#include <thread>

namespace draco
{
	class Mesh;
}

namespace teleport::core
{
	struct PrimitiveArray
	{
		size_t attributeCount;
		std::vector<avs::Attribute> attributes;
		uint64_t indices_accessor;
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
	//extern avs::AttributeSemantic FromDracoGeometryAttribute(int type, int index);
	extern avs::Accessor::DataType FromDracoNumComponents(int num_components);
	extern avs::Result DracoMeshToPrimitiveArray(avs::uid primitiveArrayUid
													, DecodedGeometry &dg
													, const draco::Mesh &dracoMesh
													, const avs::CompressedSubMesh &subMesh
													, platform::crossplatform::AxesStandard axesStandard
													, std::vector<std::vector<uint8_t>> &decompressedBuffers
													, size_t &decompressedBufferIndex);
}
