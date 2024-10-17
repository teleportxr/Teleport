#include "DecodeMesh.h"
#include "TeleportCore/Logging.h"
#include "draco/compression/decode.h"
#include "draco/io/gltf_decoder.h"

using namespace teleport::core;

#pragma optimize("",off)

namespace teleport::core
{
	avs::ComponentType FromDracoDataType(draco::DataType dracoDataType)
	{
		switch (dracoDataType)
		{
		case draco::DataType::DT_FLOAT32:
			return avs::ComponentType::FLOAT;
		case draco::DataType::DT_FLOAT64:
			return avs::ComponentType::DOUBLE;
		case draco::DataType::DT_INVALID:
			return avs::ComponentType::HALF;
		case draco::DataType::DT_INT32:
			return avs::ComponentType::UINT;
		case draco::DataType::DT_INT16:
			return avs::ComponentType::USHORT;
		case draco::DataType::DT_INT8:
			return avs::ComponentType::UBYTE;
		default:
			return avs::ComponentType::BYTE;
		};
	}
}

// Convert from Draco to our Semantic. But Draco semantics are limited. So the index helps disambiguate.
avs::AttributeSemantic FromDracoGeometryAttribute(int t, int index)
{
	switch ((draco::GeometryAttribute::Type)t)
	{
	case draco::GeometryAttribute::Type::POSITION:
		return avs::AttributeSemantic::POSITION;
	case draco::GeometryAttribute::Type::NORMAL:
		return avs::AttributeSemantic::NORMAL;
	case draco::GeometryAttribute::Type::JOINTS:
		return avs::AttributeSemantic::JOINTS_0;
	case draco::GeometryAttribute::Type::WEIGHTS:
		return avs::AttributeSemantic::WEIGHTS_0;
	case draco::GeometryAttribute::Type::GENERIC:
		if (index == 0)
			return avs::AttributeSemantic::TANGENT;
		if (index == 1)
			return avs::AttributeSemantic::JOINTS_0;
		if (index == 2)
			return avs::AttributeSemantic::WEIGHTS_0;
		if (index == 3)
			return avs::AttributeSemantic::TANGENTNORMALXZ;
		return avs::AttributeSemantic::TANGENT;
	case draco::GeometryAttribute::Type::TEX_COORD:
		if (index == 1)
			return avs::AttributeSemantic::TEXCOORD_1;
		if (index == 2)
			return avs::AttributeSemantic::TEXCOORD_2;
		if (index == 3)
			return avs::AttributeSemantic::TEXCOORD_3;
		if (index == 4)
			return avs::AttributeSemantic::TEXCOORD_4;
		if (index == 5)
			return avs::AttributeSemantic::TEXCOORD_5;
		if (index == 6)
			return avs::AttributeSemantic::TEXCOORD_6;
		return avs::AttributeSemantic::TEXCOORD_0;
	case draco::GeometryAttribute::Type::COLOR:
		return avs::AttributeSemantic::COLOR_0;
	default:
		return avs::AttributeSemantic::COUNT;
	};
}

avs::Accessor::DataType teleport::core::FromDracoNumComponents(int num_components)
{
	switch (num_components)
	{
	case 1:
		return avs::Accessor::DataType::SCALAR;
	case 2:
		return avs::Accessor::DataType::VEC2;
	case 3:
		return avs::Accessor::DataType::VEC3;
	case 4:
		return avs::Accessor::DataType::VEC4;
	default:
		return avs::Accessor::DataType::SCALAR;
		break;
	}
}

avs::Result teleport::core::DracoMeshToPrimitiveArray(avs::uid primitiveArrayUid, teleport::core::DecodedGeometry &dg
													, const draco::Mesh &dracoMesh
													, const avs::CompressedSubMesh &subMesh
													, platform::crossplatform::AxesStandard axesStandard
													, std::vector<std::vector<uint8_t>> &decompressedBuffers
													, size_t &decompressedBufferIndex)
{
	dg.axesStandard = axesStandard;
	size_t indexStride = sizeof(draco::PointIndex);
	size_t attributeCount = dracoMesh.num_attributes();
	size_t numAttributeSemantics = attributeCount;
	// Let's create ONE buffer per attribute.
	std::vector<uint64_t> buffers;
	std::vector<uint64_t> buffer_views;

	const auto *dracoPositionAttribute = dracoMesh.GetNamedAttribute(draco::GeometryAttribute::Type::POSITION);
	size_t num_vertices = dracoPositionAttribute->is_mapping_identity() ? dracoPositionAttribute->size() : dracoPositionAttribute->indices_map_size();
	for (size_t k = 0; k < attributeCount; k++)
	{
		const auto *dracoAttribute = dracoMesh.attribute((int32_t)k);
		const auto *dracoBuffer = dracoAttribute->buffer();
		uint64_t buffer_uid = dg.next_id++;
		auto &buffer = dg.buffers[buffer_uid];
		buffers.push_back(buffer_uid);
		buffer.byteLength = dracoBuffer->data_size();
		uint64_t buffer_view_uid = dg.next_id++;
		buffer_views.push_back(buffer_view_uid);
		auto &bufferView = dg.bufferViews[buffer_view_uid];
		// we're converting all attributes to float...
		bufferView.byteStride = dracoAttribute->num_components() * sizeof(float); // dracoAttribute->byte_stride();
		// This could be bigger than draco's buffer, because we want a 1-2-1 correspondence of attribute values on each vertex.
		bufferView.byteLength = num_vertices * bufferView.byteStride;
		bufferView.byteOffset = 0;
		buffer.byteLength = bufferView.byteLength;
		if (decompressedBufferIndex >= decompressedBuffers.size())
			decompressedBuffers.resize(decompressedBuffers.size() * 2);
		auto &buf = decompressedBuffers[decompressedBufferIndex++];
		buf.resize(buffer.byteLength);
		buffer.data = buf.data();

		uint8_t *buf_ptr = buffer.data;
		dracoAttribute->data_type();
		std::array<float, 4> value;
		for (draco::PointIndex i(0); i < static_cast<uint32_t>(num_vertices); ++i)
		{
			if (!dracoAttribute->ConvertValue(dracoAttribute->mapped_index(i), dracoAttribute->num_components(), &value[0]))
			{
				return avs::Result::DecoderBackend_DecodeFailed;
			}
			memcpy(buf_ptr, &value[0], bufferView.byteStride);
			buf_ptr += bufferView.byteStride;
		}
		bufferView.buffer = buffer_uid;
	}
	std::vector<avs::uid> index_buffer_uids;
	avs::uid indices_buffer_uid = dg.next_id++;
	buffers.push_back(indices_buffer_uid);
	auto &indicesBuffer = dg.buffers[indices_buffer_uid];
	size_t subMeshFaces = dracoMesh.num_faces();
	if (sizeof(draco::PointIndex) == sizeof(uint32_t))
	{
		indicesBuffer.byteLength = 3 * sizeof(uint32_t) * subMeshFaces;
	}
	else if (sizeof(draco::PointIndex) == sizeof(uint16_t))
	{
		indicesBuffer.byteLength = 3 * sizeof(uint16_t) * subMeshFaces;
	}
	indicesBuffer.data = new uint8_t[indicesBuffer.byteLength];
	uint8_t *ind_ptr = indicesBuffer.data;
	for (uint32_t j = 0; j < subMeshFaces; j++)
	{
		const draco::Mesh::Face &face = dracoMesh.face(draco::FaceIndex(subMesh.first_index / 3 + j));
		for (size_t k = 0; k < 3; k++)
		{
			uint32_t val = static_cast<uint32_t>(draco::PointIndex(face[k]).value());
			memcpy(ind_ptr, &val, indexStride);
			ind_ptr += indexStride;
		}
	}

	uint64_t indices_accessor_id = dg.next_id++;
	auto &indices_accessor = dg.accessors[indices_accessor_id];
	if (sizeof(draco::PointIndex) == sizeof(uint32_t))
		indices_accessor.componentType = avs::ComponentType::UINT;
	else
		indices_accessor.componentType = avs::ComponentType::USHORT;
	indices_accessor.bufferView = dg.next_id++;
	
	uint32_t num_indices = subMeshFaces * 3;
	indices_accessor.count = num_indices;
	indices_accessor.byteOffset = 0;
	indices_accessor.type = avs::Accessor::DataType::SCALAR;
	buffer_views.push_back(indices_accessor.bufferView);
	auto &indicesBufferView = dg.bufferViews[indices_accessor.bufferView];
	indicesBufferView.byteOffset = 0;
	indicesBufferView.byteLength = indexStride * num_indices;
	indicesBufferView.byteStride = indexStride;
	indicesBufferView.buffer = indices_buffer_uid;
	indicesBuffer.byteLength = indicesBufferView.byteLength;

	avs::PrimitiveMode primitiveMode = avs::PrimitiveMode::TRIANGLES;
	std::vector<avs::Attribute> attributes;
	attributes.reserve(attributeCount);
	std::map <avs::AttributeSemantic,uint8_t> semanticInstanceCount;
	for (int32_t k = 0; k < (int32_t)attributeCount; k++)
	{
		auto *dracoAttribute = dracoMesh.attribute((int32_t)k);
		int semantic_index=0;
		avs::AttributeSemantic semantic=FromDracoGeometryAttribute(dracoAttribute->attribute_type(),semantic_index);
		while(semanticInstanceCount[semantic]>0&&semantic_index<5)
		{
			semantic=FromDracoGeometryAttribute(dracoAttribute->attribute_type(), ++semantic_index);
		}
		if(semantic>=avs::AttributeSemantic::COUNT)
		{
			TELEPORT_WARN("Invalid semantic {0}",(int)semantic);
		}
		semanticInstanceCount[semantic]++;
		uint64_t accessor_uid = dg.next_id++;
		attributes.push_back({semantic, accessor_uid});
		auto &accessor = dg.accessors[accessor_uid];
		accessor.componentType = FromDracoDataType(dracoAttribute->data_type());
		accessor.type = FromDracoNumComponents(dracoAttribute->num_components());
		accessor.byteOffset = dracoAttribute->GetBytePos(draco::AttributeValueIndex(k));
		accessor.count = dracoAttribute->is_mapping_identity() ? dracoAttribute->size() : dracoAttribute->indices_map_size();
		accessor.bufferView = buffer_views[k];
	}
	dg.primitiveArrays[primitiveArrayUid].push_back({attributeCount, attributes, indices_accessor_id, 0, primitiveMode});
	return avs::Result::OK;
}
