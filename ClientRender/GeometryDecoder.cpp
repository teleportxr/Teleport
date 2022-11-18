//#pragma warning(4018,off)
#include "GeometryDecoder.h"

#include <fstream>
#include <iostream>
#include "Common.h"
#include "Platform/Core/FileLoader.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/AnimationInterface.h"

#ifdef _MSC_VER
#pragma warning(disable:4018;disable:4804)
#endif
#include "draco/compression/decode.h"

#define Next8B get<uint64_t>(m_Buffer.data(), &m_BufferOffset)
#define Next4B get<uint32_t>(m_Buffer.data(), &m_BufferOffset)
#define Next2B get<uint16_t>(m_Buffer.data(), &m_BufferOffset)
#define NextB get<uint8_t>(m_Buffer.data(), &m_BufferOffset)
#define NextFloat get<float>(m_Buffer.data(), &m_BufferOffset)
#define NextVec4 get<avs::vec4>(m_Buffer.data(), &m_BufferOffset)
#define NextVec3 get<avs::vec3>(m_Buffer.data(), &m_BufferOffset)
#define NextChunk(T) get<T>(m_Buffer.data(), &m_BufferOffset)  

using std::string;

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

std::vector<uint8_t> savedBuffer;
avs::Result GeometryDecoder::decodeFromFile(const std::string &filename,
	avs::GeometryPayloadType type,avs::GeometryTargetBackendInterface *target)
{
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	if(!fileLoader->FileExists(filename.c_str()))
		return avs::Result::Failed;
	void *ptr=nullptr;
	unsigned sz=0;
	m_BufferOffset = 0;
	fileLoader->AcquireFileContents(ptr,sz,filename.c_str(),false);
	m_BufferSize=sz;
	m_Buffer.resize(m_BufferSize);
	memcpy(m_Buffer.data(),ptr,m_BufferSize);
	fileLoader->ReleaseFileContents(ptr);
	m_Buffer.resize(m_BufferSize);
	return decode( type, target,false);
}

avs::Result GeometryDecoder::decode(const void* buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target)
{
	// No m_GALU on the header or tail on the incoming buffer!
	m_BufferSize = bufferSizeInBytes;
	m_BufferOffset = 0;
	m_Buffer.resize(m_BufferSize);
 	memcpy(m_Buffer.data(), (uint8_t*)buffer, m_BufferSize);
	return decode(type,target,true);
}

avs::Result GeometryDecoder::decode(avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target,bool save_to_disk)
{
	size_t bufferSizeK=(m_BufferSize+1023)/1024;
	switch(type)
	{
	case avs::GeometryPayloadType::Mesh:
		//TELEPORT_COUT << "avs::GeometryPayloadType::Mesh: " << bufferSizeK << std::endl;
		return decodeMesh(target,save_to_disk);
	case avs::GeometryPayloadType::Material:
		//TELEPORT_COUT << "avs::GeometryPayloadType::Material: " << bufferSizeK << std::endl;
		return decodeMaterial(target,save_to_disk);
	case avs::GeometryPayloadType::MaterialInstance:
		return decodeMaterialInstance(target,save_to_disk);
	case avs::GeometryPayloadType::Texture:
		//TELEPORT_COUT << "avs::GeometryPayloadType::Texture: " << bufferSizeK << std::endl;
		return decodeTexture(target,save_to_disk);
	case avs::GeometryPayloadType::Animation:
		//TELEPORT_COUT << "avs::GeometryPayloadType::Animation: " << bufferSizeK << std::endl;
		return decodeAnimation(target,save_to_disk);
	case avs::GeometryPayloadType::Node:
		//TELEPORT_COUT << "avs::GeometryPayloadType::Node: " << bufferSizeK << std::endl;
		return decodeNode(target);
	case avs::GeometryPayloadType::Skin:
		//TELEPORT_COUT << "avs::GeometryPayloadType::Skin: " << bufferSizeK << std::endl;
		return decodeSkin(target,save_to_disk);
	default:
		TELEPORT_BREAK_ONCE("Invalid Geometry payload");
		return avs::Result::GeometryDecoder_InvalidPayload;
	};
}


avs::Accessor::ComponentType FromDracoDataType(draco::DataType dracoDataType)
{
	switch (dracoDataType)
	{
	case draco::DataType::DT_FLOAT32:
		return avs::Accessor::ComponentType::FLOAT;
	case draco::DataType::DT_FLOAT64:
		return avs::Accessor::ComponentType::DOUBLE;
	case draco::DataType::DT_INVALID:
		return avs::Accessor::ComponentType::HALF;
	case draco::DataType::DT_INT32:
		return avs::Accessor::ComponentType::UINT;
	case draco::DataType::DT_INT16:
		return avs::Accessor::ComponentType::USHORT;
	case draco::DataType::DT_INT8:
		return avs::Accessor::ComponentType::UBYTE;
	default:
		return avs::Accessor::ComponentType::BYTE;
	};
}

// Convert from Draco to our Semantic. But Draco semantics are limited. So the index helps disambiguate.
avs::AttributeSemantic FromDracoGeometryAttribute(draco::GeometryAttribute::Type t,int index)
{
	switch (t)
	{
	case draco::GeometryAttribute::Type::POSITION:
		return avs::AttributeSemantic::POSITION;
	case draco::GeometryAttribute::Type::NORMAL:
		return avs::AttributeSemantic::NORMAL;
	case draco::GeometryAttribute::Type::GENERIC:
		if (index == 2)
			return avs::AttributeSemantic::TANGENT;
		if (index == 6)
			return avs::AttributeSemantic::JOINTS_0;
		if (index == 7)
			return avs::AttributeSemantic::WEIGHTS_0;
		if (index == 8)
			return avs::AttributeSemantic::TANGENTNORMALXZ;
		return avs::AttributeSemantic::TANGENT;
	case draco::GeometryAttribute::Type::TEX_COORD:
		if(index==4)
			return avs::AttributeSemantic::TEXCOORD_1;
		return avs::AttributeSemantic::TEXCOORD_0;
	case draco::GeometryAttribute::Type::COLOR:
		return avs::AttributeSemantic::COLOR_0;
	default:
		return avs::AttributeSemantic::COUNT;
	};
}

avs::Accessor::DataType FromDracoNumComponents(int num_components)
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

// NOTE the inefficiency here, we're coding into "DecodedGeometry", but that is then immediately converted to a MeshCreate.
avs::Result GeometryDecoder::DracoMeshToDecodedGeometry(avs::uid primitiveArrayUid
													,DecodedGeometry &dg
													, const avs::CompressedMesh &compressedMesh)
{
	size_t primitiveArraysSize = compressedMesh.subMeshes.size();
	dg.primitiveArrays[primitiveArrayUid].reserve(primitiveArraysSize);
	size_t indexStride = sizeof(draco::PointIndex);
	uint64_t next_uid=0;
	for (size_t i = 0; i < primitiveArraysSize; i++)
	{
		auto& subMesh = compressedMesh.subMeshes[i];

		draco::Decoder dracoDecoder;
		draco::Mesh dracoMesh;
		draco::DecoderBuffer dracoDecoderBuffer;
		dracoDecoderBuffer.Init((const char*)subMesh.buffer.data(), subMesh.buffer.size());
		draco::Status dracoStatus = dracoDecoder.DecodeBufferToGeometry(&dracoDecoderBuffer, &dracoMesh);
		if (!dracoStatus.ok())
		{
			TELEPORT_CERR << "Draco decode failed: " << (uint32_t)dracoStatus.code() << std::endl;
			return avs::Result::DecoderBackend_DecodeFailed;
		}

		size_t attributeCount = dracoMesh.num_attributes();
		// Let's create ONE buffer per attribute.
		std::vector<uint64_t> buffers;
		std::vector<uint64_t> buffer_views;
		const auto* dracoPositionAttribute = dracoMesh.GetNamedAttribute(draco::GeometryAttribute::Type::POSITION);
		for (size_t k = 0; k < attributeCount; k++)
		{
			const auto* dracoAttribute = dracoMesh.attribute((int32_t)k);
			uint64_t buffer_uid = next_uid++;
			auto& buffer = dg.buffers[buffer_uid];
			buffers.push_back(buffer_uid);
			buffer.byteLength = dracoAttribute->buffer()->data_size();
			uint64_t buffer_view_uid = next_uid++;
			buffer_views.push_back(buffer_view_uid);
			auto& bufferView = dg.bufferViews[buffer_view_uid];
			bufferView.byteStride = dracoAttribute->byte_stride();
			bufferView.byteLength = dracoAttribute->buffer()->data_size();
			bufferView.byteOffset = 0;
			buffer.byteLength = bufferView.byteLength;
			if(m_DecompressedBufferIndex>=m_DecompressedBuffers.size())
				m_DecompressedBuffers.resize(m_DecompressedBuffers.size()*2);
			auto &buf=m_DecompressedBuffers[m_DecompressedBufferIndex++];
			buf.resize(buffer.byteLength);
			buffer.data = buf.data();
		

			uint8_t * buf_ptr=buffer.data;
			std::array<float, 4> value;
			for (draco::AttributeValueIndex i(0); i < static_cast<uint32_t>(dracoAttribute->size()); ++i)
			{
				if (!dracoAttribute->ConvertValue(i, dracoAttribute->num_components(), &value[0]))
				{
					return avs::Result::DecoderBackend_DecodeFailed;
				}
				memcpy(buf_ptr,&value[0], bufferView.byteStride);
				buf_ptr+=bufferView.byteStride;
			}
			bufferView.buffer = buffer_uid;
		}
		std::vector<avs::uid> index_buffer_uids;
		avs::uid indices_buffer_uid = next_uid++;
		buffers.push_back(indices_buffer_uid);
		auto& indicesBuffer = dg.buffers[indices_buffer_uid];
		size_t subMeshFaces= dracoMesh.num_faces(); ;//subMesh.num_indices / 3;
		if(sizeof(draco::PointIndex)==sizeof(uint32_t))
		{
			indicesBuffer.byteLength=3*sizeof(uint32_t)* subMeshFaces;
		}
		else if (sizeof(draco::PointIndex) == sizeof(uint16_t))
		{
			indicesBuffer.byteLength = 3 * sizeof(uint16_t) * subMeshFaces;
		}
		indicesBuffer.data = new uint8_t[indicesBuffer.byteLength];
		uint8_t * ind_ptr=indicesBuffer.data;
		for(uint32_t j=0;j<subMeshFaces;j++)
		{
			const draco::Mesh::Face& face = dracoMesh.face(draco::FaceIndex(subMesh.first_index/3+j));
			for(size_t k=0;k<3;k++)
			{
				uint32_t val= dracoPositionAttribute->mapped_index(draco::PointIndex(face[k])).value();
				memcpy(ind_ptr,&val, indexStride);
				ind_ptr+=indexStride;
			}
		}

		uint64_t indices_accessor_uid = subMesh.indices_accessor;
		auto & indices_accessor =dg.accessors[indices_accessor_uid];
		if (sizeof(draco::PointIndex) == sizeof(uint32_t))
			indices_accessor.componentType=avs::Accessor::ComponentType::UINT;
		else
			indices_accessor.componentType = avs::Accessor::ComponentType::USHORT;
		indices_accessor.bufferView = next_uid++;
		indices_accessor.count=subMesh.num_indices;
		indices_accessor.byteOffset=0;
		indices_accessor.type=avs::Accessor::DataType::SCALAR;
		buffer_views.push_back(indices_accessor.bufferView);
		auto& indicesBufferView = dg.bufferViews[indices_accessor.bufferView];
		indicesBufferView.byteOffset= 0;// indexStride *subMesh.first_index;
		indicesBufferView.byteLength= indexStride *subMesh.num_indices;
		indicesBufferView.byteStride= indexStride;
		indicesBufferView.buffer	= indices_buffer_uid ;
		indicesBuffer.byteLength	= indicesBufferView.byteLength;

		avs::PrimitiveMode primitiveMode = avs::PrimitiveMode::TRIANGLES;
		std::vector<avs::Attribute> attributes;
		attributes.reserve(attributeCount);
		for (int32_t k = 0; k < (int32_t)attributeCount; k++)
		{
			auto *dracoAttribute= dracoMesh.attribute((int32_t)k);
			const auto &a= subMesh.attributeSemantics.find(k);
			if(a== subMesh.attributeSemantics.end())
				continue;
			avs::AttributeSemantic semantic = a->second;
			uint64_t accessor_uid = next_uid++;
			attributes.push_back({ semantic, accessor_uid });
			auto &accessor=dg.accessors[accessor_uid];
			accessor.componentType=FromDracoDataType(dracoAttribute->data_type());
			accessor.type=FromDracoNumComponents(dracoAttribute->num_components());
			accessor.byteOffset=0;
			accessor.count=dracoAttribute->size();
			accessor.bufferView=buffer_views[k];
		}
		dg.primitiveArrays[primitiveArrayUid].push_back({ attributeCount, attributes, indices_accessor_uid, subMesh.material, primitiveMode });
	}
	return avs::Result::OK;
}


void GeometryDecoder::saveBuffer(const std::string &filename) const
{
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	string f=cacheFolder.length()?(cacheFolder+"/")+filename:filename;
	fileLoader->Save((const void*)m_Buffer.data(),(unsigned)m_Buffer.size(),f.c_str(),false);
/*	std::ofstream ofs(filename.c_str(),std::ofstream::out|std::ofstream::binary);
	ofs.write((const char *)m_Buffer.data(), m_Buffer.size());
	ofs.close();*/
}

void GeometryDecoder::setCacheFolder(const std::string &f)
{
	cacheFolder=f;
}

avs::Result GeometryDecoder::decodeMesh(avs::GeometryTargetBackendInterface*& target,bool save_to_disk)
{
	//Parse buffer and fill struct DecodedGeometry
	DecodedGeometry dg = {};
	dg.clear();
	avs::uid uid;

	std::string name;

	size_t meshCount = Next8B;
	m_DecompressedBuffers.clear();
	const size_t MAX_ATTR_COUNT=20;
	if(meshCount*MAX_ATTR_COUNT>=m_DecompressedBuffers.size())
		m_DecompressedBuffers.resize(meshCount*MAX_ATTR_COUNT);
	m_DecompressedBufferIndex=0;
	for (size_t i = 0; i < meshCount; i++)
	{
		uid = Next8B;
		avs::CompressedMesh compressedMesh;
		compressedMesh.meshCompressionType =(avs::MeshCompressionType)NextB;
		//if(avs::uid==12)
		if(compressedMesh.meshCompressionType ==avs::MeshCompressionType::DRACO)
		{
			int32_t version_number= Next4B;
			size_t nameLength = Next8B;
			name.resize(nameLength);
			copy<char>(name.data(), m_Buffer.data(), m_BufferOffset, nameLength);
			compressedMesh.name= name;
			if(save_to_disk)
				saveBuffer(std::string("meshes/"+name+".mesh_compressed"));
			size_t num_elements=(size_t)Next4B;
			compressedMesh.subMeshes.resize(num_elements);
			for(size_t i=0;i< num_elements;i++)
			{
				auto &subMesh= compressedMesh.subMeshes[i];
				subMesh.indices_accessor=Next8B;
				subMesh.material=Next8B;
				subMesh.first_index = Next4B;
				subMesh.num_indices = Next4B;
				size_t numAttributeSemantics = Next8B;
				for (size_t i = 0; i < numAttributeSemantics; i++)
				{
					int32_t attr= Next4B;
					subMesh.attributeSemantics[attr] = (avs::AttributeSemantic)NextB;
				}
				size_t bufferSize = Next8B;
				subMesh.buffer.resize(bufferSize);
				copy<uint8_t>(subMesh.buffer.data(), m_Buffer.data(), m_BufferOffset, bufferSize);
			}
			avs::Result result = DracoMeshToDecodedGeometry(uid, dg, compressedMesh);
			if (result != avs::Result::OK)
				return result;
		}
		else if(compressedMesh.meshCompressionType ==avs::MeshCompressionType::NONE)
		{
			int32_t version_number= Next4B;
			size_t nameLength = Next8B;
			name.resize(nameLength);
			copy<char>(name.data(), m_Buffer.data(), m_BufferOffset, nameLength);
			compressedMesh.name= name;
			size_t primitiveArraysSize = Next8B;
			dg.primitiveArrays[uid].reserve(primitiveArraysSize);

			for (size_t j = 0; j < primitiveArraysSize; j++)
			{
				size_t attributeCount = Next8B;
				avs::uid indices_accessor = Next8B;
				avs::uid material = Next8B;
				avs::PrimitiveMode primitiveMode = (avs::PrimitiveMode)Next4B;

				std::vector<avs::Attribute> attributes;
				attributes.reserve(attributeCount);
				for (size_t k = 0; k < attributeCount; k++)
				{
					avs::AttributeSemantic semantic = (avs::AttributeSemantic)Next8B;
					avs::uid accessor = Next8B;
					attributes.push_back({ semantic, accessor });
				}

				dg.primitiveArrays[uid].push_back({ attributeCount, attributes, indices_accessor, material, primitiveMode });
			}
			bool isIndexAccessor = true;
			size_t accessorsSize = Next8B;
			for (size_t j = 0; j < accessorsSize; j++)
			{
				avs::uid acc_uid= Next8B;
				avs::Accessor::DataType type = (avs::Accessor::DataType)Next4B;
				avs::Accessor::ComponentType componentType = (avs::Accessor::ComponentType)Next4B;
				size_t count = Next8B;
				avs::uid bufferView = Next8B;
				size_t byteOffset = Next8B;

				if (isIndexAccessor) // For Indices Only
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

				dg.buffers[key].data = (m_Buffer.data() + m_BufferOffset);
				m_BufferOffset += dg.buffers[key].byteLength;
			}
		}
		else
		{
			TELEPORT_CERR << "Unknown meshCompressionType: " << (uint32_t)compressedMesh.meshCompressionType << std::endl;
			return avs::Result::DecoderBackend_DecodeFailed;
		}
	}
	// TODO: Is there any point in FIRST creating DecodedGeometry THEN translating that to MeshCreate, THEN using MeshCreate to
	// 	   create the mesh? Why not go direct to MeshCreate??
	// dg is complete, now send to avs::GeometryTargetBackendInterface
	for (std::unordered_map<avs::uid, std::vector<PrimitiveArray2>>::iterator it = dg.primitiveArrays.begin(); it != dg.primitiveArrays.end(); it++)
	{
		size_t index = 0;
		avs::MeshCreate meshCreate;
		meshCreate.mesh_uid = it->first;
		meshCreate.m_MeshElementCreate.resize(it->second.size());
		// Primitive array elements in each mesh.
		for (const auto& primitive : it->second)
		{
			avs::MeshElementCreate& meshElementCreate = meshCreate.m_MeshElementCreate[index];
			meshElementCreate.vb_id = primitive.attributes[0].accessor;
			size_t vertexCount = 0;
			for (size_t i = 0; i < primitive.attributeCount; i++)
			{
				//Vertices
				const avs::Attribute& attrib = primitive.attributes[i];
				const avs::Accessor& accessor = dg.accessors[attrib.accessor];
				const avs::BufferView& bufferView = dg.bufferViews[accessor.bufferView];
				const avs::GeometryBuffer& buffer = dg.buffers[bufferView.buffer];
				const uint8_t* data = buffer.data + bufferView.byteOffset;

				switch (attrib.semantic)
				{
				case avs::AttributeSemantic::POSITION:
					meshElementCreate.m_VertexCount = vertexCount = accessor.count;
					meshElementCreate.m_Vertices = reinterpret_cast<const avs::vec3*>(data);
					continue;
				case avs::AttributeSemantic::TANGENTNORMALXZ:
				{
					size_t tnSize = 0;
					tnSize = avs::GetComponentSize(accessor.componentType) * avs::GetDataTypeSize(accessor.type);
					meshElementCreate.m_TangentNormalSize = tnSize;
					meshElementCreate.m_TangentNormals = reinterpret_cast<const uint8_t*>(data);
				}
				continue;
				case avs::AttributeSemantic::NORMAL:
					meshElementCreate.m_Normals = reinterpret_cast<const avs::vec3*>(data);
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::TANGENT:
					meshElementCreate.m_Tangents = reinterpret_cast<const avs::vec4*>(data);
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::TEXCOORD_0:
					meshElementCreate.m_UV0s = reinterpret_cast<const avs::vec2*>(data);
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::TEXCOORD_1:
					meshElementCreate.m_UV1s = reinterpret_cast<const avs::vec2*>(data);
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::COLOR_0:
					meshElementCreate.m_Colors = reinterpret_cast<const avs::vec4*>(data);
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::JOINTS_0:
					meshElementCreate.m_Joints = reinterpret_cast<const avs::vec4*>(data);
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::WEIGHTS_0:
					meshElementCreate.m_Weights = reinterpret_cast<const avs::vec4*>(data);
					assert(accessor.count == vertexCount);
					continue;
				default:
					TELEPORT_CERR << "Unknown attribute semantic: " << (uint32_t)attrib.semantic << std::endl;
					continue;
				}
			}

			//Indices
			const avs::Accessor& indicesAccessor = dg.accessors[primitive.indices_accessor];
			const avs::BufferView& indicesBufferView = dg.bufferViews[indicesAccessor.bufferView];
			const avs::GeometryBuffer& indicesBuffer = dg.buffers[indicesBufferView.buffer];
			size_t componentSize = avs::GetComponentSize(indicesAccessor.componentType);
			meshElementCreate.ib_id = primitive.indices_accessor;
			meshElementCreate.m_Indices = (indicesBuffer.data + indicesBufferView.byteOffset + indicesAccessor.byteOffset);
			meshElementCreate.m_IndexSize = componentSize;
			meshElementCreate.m_IndexCount = indicesAccessor.count;
			meshElementCreate.m_ElementIndex = index;
			index++;
		}
		meshCreate.name = name;

		avs::Result result = target->CreateMesh(meshCreate);
		if (result != avs::Result::OK)
		{
			return result;
		}
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeMaterial(avs::GeometryTargetBackendInterface*& target,bool save_to_disk)
{
	size_t materialAmount = Next8B;

	for(size_t i = 0; i < materialAmount; i++)
	{
		avs::Material material;
		
		avs::uid mat_uid = Next8B;

		size_t nameLength = Next8B;
		
		material.name.resize(nameLength);
		copy<char>(material.name.data(), m_Buffer.data(), m_BufferOffset, nameLength);
		material.materialMode = (avs::MaterialMode)NextB;
		material.pbrMetallicRoughness.baseColorTexture.index = Next8B;
		TELEPORT_COUT <<"GeometryDecoder::decodeMaterial - "<<mat_uid<<" "<< material.name.c_str() << " diffuse " << material.pbrMetallicRoughness.baseColorTexture.index << std::endl;
		material.pbrMetallicRoughness.baseColorTexture.texCoord = NextB;
		material.pbrMetallicRoughness.baseColorTexture.tiling.x = NextFloat;
		material.pbrMetallicRoughness.baseColorTexture.tiling.y = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.x = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.y = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.z = NextFloat;
		material.pbrMetallicRoughness.baseColorFactor.w = NextFloat;

		material.pbrMetallicRoughness.metallicRoughnessTexture.index = Next8B;
		material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = NextB;
		material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.x = NextFloat;
		material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.y = NextFloat;
		material.pbrMetallicRoughness.metallicFactor = NextFloat;
		material.pbrMetallicRoughness.roughnessMultiplier = NextFloat;
		material.pbrMetallicRoughness.roughnessOffset = NextFloat;

		material.normalTexture.index = Next8B;
		material.normalTexture.texCoord = NextB;
		material.normalTexture.tiling.x = NextFloat;
		material.normalTexture.tiling.y = NextFloat;
		material.normalTexture.scale = NextFloat;

		material.occlusionTexture.index = Next8B;
		material.occlusionTexture.texCoord = NextB;
		material.occlusionTexture.tiling.x = NextFloat;
		material.occlusionTexture.tiling.y = NextFloat;
		material.occlusionTexture.strength = NextFloat;

		material.emissiveTexture.index = Next8B;
		material.emissiveTexture.texCoord = NextB;
		material.emissiveTexture.tiling.x = NextFloat;
		material.emissiveTexture.tiling.y = NextFloat;
		material.emissiveFactor.x = NextFloat;
		material.emissiveFactor.y = NextFloat;
		material.emissiveFactor.z = NextFloat;

		size_t extensionAmount = Next8B;
		for(size_t i = 0; i < extensionAmount; i++)
		{
			std::unique_ptr<avs::MaterialExtension> newExtension;
			avs::MaterialExtensionIdentifier id = static_cast<avs::MaterialExtensionIdentifier>(Next4B);

			switch(id)
			{
				case avs::MaterialExtensionIdentifier::SIMPLE_GRASS_WIND:
					newExtension = std::make_unique<avs::SimpleGrassWindExtension>();
					newExtension->deserialise(m_Buffer, m_BufferOffset);
					break;
			}

			material.extensions[id] = std::move(newExtension);
		}

		target->CreateMaterial(mat_uid, material);
	}
	
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeMaterialInstance(avs::GeometryTargetBackendInterface*& target,bool save_to_disk)
{
	return avs::Result::GeometryDecoder_Incomplete;
}

avs::Result GeometryDecoder::decodeTexture(avs::GeometryTargetBackendInterface*& target,bool save_to_disk)
{
	size_t textureAmount = Next8B;
	for(size_t i = 0; i < textureAmount; i++)
	{
		avs::Texture texture;
		avs::uid texture_uid = Next8B;

		size_t nameLength = Next8B;
		texture.name.resize(nameLength);
		copy<char>(texture.name.data(), m_Buffer.data(), m_BufferOffset, nameLength);
		
		texture.cubemap= NextB!=0;

		texture.width = Next4B;
		texture.height = Next4B;
		texture.depth = Next4B;
		texture.bytesPerPixel = Next4B;
		texture.arrayCount = Next4B;
		texture.mipCount = Next4B;
		texture.format = static_cast<avs::TextureFormat>(Next4B);
		if(texture.format == avs::TextureFormat::INVALID)
			texture.format = avs::TextureFormat::G8;
		texture.compression = static_cast<avs::TextureCompression>(Next4B);
		texture.valueScale = NextFloat;

		texture.dataSize = Next4B;
		texture.data = (m_Buffer.data() + m_BufferOffset);

		texture.sampler_uid = Next8B;

		target->CreateTexture(texture_uid, texture);
	}

	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeAnimation(avs::GeometryTargetBackendInterface*& target,bool save_to_disk)
{
	avs::Animation animation;
	avs::uid animationID = Next8B;
	size_t nameLength = Next8B;
	animation.name.resize(nameLength);
	copy<char>(animation.name.data(), m_Buffer.data(), m_BufferOffset, nameLength);
	if(save_to_disk)
		saveBuffer(std::string("animations/"+animation.name+".anim"));

	animation.boneKeyframes.resize(Next8B);
	for(size_t i = 0; i < animation.boneKeyframes.size(); i++)
	{
		avs::TransformKeyframeList& transformKeyframe = animation.boneKeyframes[i];
		transformKeyframe.boneIndex = Next8B;

		decodeVector3Keyframes(transformKeyframe.positionKeyframes);
		decodeVector4Keyframes(transformKeyframe.rotationKeyframes);
	}

	target->CreateAnimation(animationID, animation);

	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeNode(avs::GeometryTargetBackendInterface*& target)
{
	uint64_t nodeCount = Next8B;
	for(uint64_t i = 0; i < nodeCount; ++i)
	{
		avs::uid uid = Next8B;

		avs::Node node;

		size_t nameLength = Next8B;
		node.name.resize(nameLength);
		copy<char>(node.name.data(), m_Buffer.data(), m_BufferOffset, nameLength);

		node.localTransform = NextChunk(avs::Transform);
		node.globalTransform = NextChunk(avs::Transform);
		bool useLocalTransform =(NextB)!=0;

		node.stationary =(NextB)!=0;
		node.holder_client_id = Next8B;
		node.priority = Next4B;
		node.data_uid = Next8B;
		node.data_type = static_cast<avs::NodeDataType>(NextB);

		node.skinID = Next8B;
		node.parentID = Next8B;

		node.animations.resize(Next8B);
		for(size_t j = 0; j < node.animations.size(); j++)
		{
			node.animations[j] = Next8B;
		}

		switch(node.data_type)
		{
			case avs::NodeDataType::Mesh:
			{
				uint64_t materialCount = Next8B;
				node.materials.reserve(materialCount);
				for(uint64_t j = 0; j < materialCount; ++j)
				{
					node.materials.push_back(Next8B);
				}
				node.renderState.lightmapScaleOffset=NextVec4;
				node.renderState.globalIlluminationUid = Next8B;
			}
			break;
			case avs::NodeDataType::Light:
				node.lightColour = NextVec4;
				node.lightRadius = NextFloat;
				node.lightRange = NextFloat;
				node.lightDirection = NextVec3;
				node.lightType = NextB;
				break;
			default:
				break;
		};

		uint64_t childCount = Next8B;
		node.childrenIDs.reserve(childCount);
		for(uint64_t j = 0; j < childCount; ++j)
		{
			node.childrenIDs.push_back(Next8B);
		}

		target->CreateNode(uid, node);
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeSkin(avs::GeometryTargetBackendInterface*& target,bool save_to_disk)
{
	avs::uid skinID = Next8B;

	avs::Skin skin;

	size_t nameLength = Next8B;
	skin.name.resize(nameLength);
	copy<char>(skin.name.data(), m_Buffer.data(), m_BufferOffset, nameLength);
	if(save_to_disk)
		saveBuffer(std::string("skins/"+skin.name+".skin"));

	skin.inverseBindMatrices.resize(Next8B);
	for(size_t i = 0; i < skin.inverseBindMatrices.size(); i++)
	{
		skin.inverseBindMatrices[i] = NextChunk(avs::Mat4x4);
	}

	#if 0
	skin.boneIDs.resize(Next8B);
	for (size_t i = 0; i < skin.boneIDs.size(); i++)
	{
		skin.boneIDs[i] = Next8B;
	}

	skin.jointIDs.resize(Next8B);
	for(size_t i = 0; i < skin.jointIDs.size(); i++)
	{
		skin.jointIDs[i] = Next8B;
	}
	#else
	skin.boneTransforms.resize(Next8B);
	skin.parentIndices.resize(skin.boneTransforms.size());
	skin.boneNames.resize(skin.boneTransforms.size());
	for (size_t i = 0; i < skin.boneTransforms.size(); i++)
	{
		skin.parentIndices[i]=Next2B;
		skin.boneTransforms[i] = NextChunk(avs::Transform);
		size_t nameLength = Next8B;
		skin.boneNames[i].resize(nameLength);
		copy<char>(skin.boneNames[i].data(), m_Buffer.data(), m_BufferOffset, nameLength);
	}
	skin.jointIndices.resize(Next8B);
	for (size_t i = 0; i < skin.jointIndices.size(); i++)
	{
		skin.jointIndices[i]=Next2B;
	}
	#endif
	skin.skinTransform = NextChunk(avs::Transform);

	target->CreateSkin(skinID, skin);
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeFloatKeyframes(std::vector<avs::FloatKeyframe>& keyframes)
{
	keyframes.resize(Next8B);
	for(size_t i = 0; i < keyframes.size(); i++)
	{
		keyframes[i].time = NextFloat;
		keyframes[i].value = NextFloat;
	}

	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeVector3Keyframes(std::vector<avs::Vector3Keyframe>& keyframes)
{
	keyframes.resize(Next8B);
	for(size_t i = 0; i < keyframes.size(); i++)
	{
		keyframes[i].time = NextFloat;
		keyframes[i].value = NextChunk(avs::vec3);
	}

	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeVector4Keyframes(std::vector<avs::Vector4Keyframe>& keyframes)
{
	keyframes.resize(Next8B);
	for(size_t i = 0; i < keyframes.size(); i++)
	{
		keyframes[i].time = NextFloat;
		keyframes[i].value = NextChunk(avs::vec4);
	}

	return avs::Result::OK;
}
