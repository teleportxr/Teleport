// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>
#include <libavstream/common_networking.h>
#include <libavstream/node.h>

#include "material_interface.hpp"

// TODO: don't forward-reference any teleport::core things from avs.
namespace teleport::core
{
	struct Animation;
}
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
namespace avs
{
	extern void AVSTREAM_API ConvertTransform(AxesStandard fromStandard, AxesStandard toStandard, Transform &transform);
	extern void AVSTREAM_API ConvertRotation(AxesStandard fromStandard, AxesStandard toStandard, vec4 &rotation);
	extern void AVSTREAM_API ConvertPosition(AxesStandard fromStandard, AxesStandard toStandard, vec3 &position);
	extern void AVSTREAM_API ConvertScale(AxesStandard fromStandard, AxesStandard toStandard, vec3 &scale);
	extern int8_t AVSTREAM_API ConvertAxis(AxesStandard fromStandard, AxesStandard toStandard, int8_t axis);
	

	inline size_t GetDataTypeSize(Accessor::DataType t)
	{
		switch (t)
		{
		case Accessor::DataType::SCALAR:
			return 1;
		case Accessor::DataType::VEC2:
			return 2;
		case Accessor::DataType::VEC3:
			return 3;
		case Accessor::DataType::MAT4:
			return 16;
		case Accessor::DataType::VEC4:
		default:
			return 4;
		};
	}

	struct Skeleton
	{
		std::string name;
		std::vector<uid> boneIDs;
		Transform skeletonTransform;
		// New method, bones are built into the skeleton:
		std::vector<Transform> boneTransforms;
		std::vector<int16_t> parentIndices;
		std::vector<std::string> boneNames;

		static Skeleton convertToStandard(const Skeleton& skeleton, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
		{
			avs::Skeleton convertedSkeleton;
			convertedSkeleton.name = skeleton.name;
			convertedSkeleton.boneIDs = skeleton.boneIDs;

			convertedSkeleton.skeletonTransform = skeleton.skeletonTransform;
			avs::ConvertTransform(sourceStandard, targetStandard, convertedSkeleton.skeletonTransform);

			return convertedSkeleton;
		}
	};
	template<typename U,typename T>
	bool operator==(const std::map<U, T>& m1, const std::map<U, T>& m2)
	{
		if (m1.size() != m2.size())
			return false;
		for (auto a : m1)
		{
			auto b = m2.find(a.first);
			if (b == m2.end())
				return false;
			if (!(a.second==b->second))
				return false;
		}
		return true;
	}
	template< typename T>
	bool operator==(const std::vector<T> &m1, const std::vector<T> &m2)
	{
		if (m1.size() != m2.size())
			return false;
		for (size_t i=0;i<m1.size();i++)
		{
			if (!(m1[i] ==m2[i]))
				return false;
		}
		return true;
	}
	struct Mesh
	{
		std::string name;

		std::vector<PrimitiveArray> primitiveArrays;
		std::unordered_map<uint64_t, Accessor> accessors;
		std::map<uint64_t, BufferView> bufferViews;
		std::map<uint64_t, GeometryBuffer> buffers;
		uint64_t inverseBindMatricesAccessorID;

		inline std::tuple<const uint8_t*,size_t> GetDataFromAccessor(uint64_t id) const
		{
			auto a=accessors.find(id);
			if(a==accessors.end())
				return {0,0};
			auto bv=bufferViews.find(a->second.bufferView);
			if(bv==bufferViews.end())
				return {0,0};
			size_t offs=a->second.byteOffset;
			size_t sz=avs::GetDataTypeSize(a->second.type);
			sz*=avs::GetComponentSize(a->second.componentType);
			sz*=a->second.count;

			auto b=buffers.find(bv->second.buffer);
			if(b==buffers.end())
				return {0,0};
			uint8_t *data=b->second.data+bv->second.byteOffset+a->second.byteOffset;
			return {data, sz};
		}
		bool operator==(const Mesh& m) const
		{
			if (primitiveArrays != m.primitiveArrays)
				return false;
			for (auto a : accessors)
			{
				auto b = m.accessors.find(a.first);
				if (b == m.accessors.end())
					return false;
				if (!a.second.operator==(b->second))
					return false;
			}
			if (bufferViews != m.bufferViews)
				return false;
			if (buffers != m.buffers)
				return false;
			if (inverseBindMatricesAccessorID != m.inverseBindMatricesAccessorID)
				return false;
			return true;
		}
		void ResetAccessors(uint64_t subtract)
		{
			if(subtract<=1)
				return;
			subtract--;
			for(auto &a:primitiveArrays)
			{
				a.indices_accessor-=subtract;
			}
			std::unordered_map<uint64_t, Accessor> accessors_new;
			for( auto &c:accessors)
			{
				c.second.bufferView-=subtract;
				accessors_new[c.first-subtract]=c.second;
			}
			accessors=accessors_new;
			std::map<uint64_t, BufferView> bufferViews_new;
			for( auto &v:bufferViews)
			{
				v.second.buffer-=subtract;
				bufferViews_new[v.first-subtract]=v.second;
			}
			bufferViews=bufferViews_new;
			std::map<uint64_t, GeometryBuffer> buffers_new;
			for(auto &b:buffers)
			{
				buffers_new[b.first-subtract]=b.second;
			}
			buffers=buffers_new;
			inverseBindMatricesAccessorID-=subtract;
		}
		void GetAccessorRange(uint64_t &lowest,uint64_t &highest) const
		{
			for(const auto &a:primitiveArrays)
			{
				lowest=std::min(lowest,a.indices_accessor);
				highest=std::max(highest,a.indices_accessor);
			}
			for(const auto &c:accessors)
			{
				lowest=std::min(lowest,(uint64_t)c.first);
				highest=std::max(highest,(uint64_t)c.first);
				lowest=std::min(lowest,(uint64_t)c.second.bufferView);
				highest=std::max(highest,(uint64_t)c.second.bufferView);
			}
			for(const auto &v:bufferViews)
			{
				lowest=std::min(lowest,(uint64_t)v.first);
				highest=std::max(highest,(uint64_t)v.first);
				lowest=std::min(lowest,(uint64_t)v.second.buffer);
				highest=std::max(highest,(uint64_t)v.second.buffer);
			}
			for(const auto &b:buffers)
			{
				lowest=std::min(lowest,(uint64_t)b.first);
				highest=std::max(highest,(uint64_t)b.first);
			}
			lowest=std::min(lowest,(uint64_t)inverseBindMatricesAccessorID);
			highest=std::max(highest,(uint64_t)inverseBindMatricesAccessorID);
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Mesh& mesh)
		{
			//Name needs its own line, so spaces can be included.
			out << mesh.name;

			out.writeChunk(mesh.primitiveArrays.size());
			for(size_t i = 0; i < mesh.primitiveArrays.size(); i++)
			{
				out  << mesh.primitiveArrays[i];
			}
			out.writeChunk(mesh.accessors.size());
			for(auto& accessorPair : mesh.accessors)
			{
				out.writeChunk(accessorPair.first);
				out<< accessorPair.second;
			}

			out.writeChunk(mesh.bufferViews.size());
			for(auto& bufferViewPair : mesh.bufferViews)
			{
				out.writeChunk(bufferViewPair.first);
				out <<  bufferViewPair.second;
			}
			out.writeChunk(mesh.buffers.size());
			for(auto& bufferPair : mesh.buffers)
			{
				out.writeChunk(bufferPair.first);
				out << bufferPair.second;
			}
			out.writeChunk(mesh.inverseBindMatricesAccessorID);
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, Mesh& mesh)
		{
			in>>mesh.name;

			size_t primitiveArrayCount;
			in.readChunk(primitiveArrayCount);
			mesh.primitiveArrays.resize(primitiveArrayCount);
			for(size_t i = 0; i < primitiveArrayCount; i++)
			{
				in >> mesh.primitiveArrays[i];
			}
			size_t accessorCount;
			in.readChunk(accessorCount);
			for(size_t i = 0; i < accessorCount; i++)
			{
				uint64_t id;
				avs::Accessor accessor;
				in.readChunk(id);
				in >> accessor;
				mesh.accessors[id] = accessor;
			}

			size_t bufferViewCount;
			in.readChunk(bufferViewCount);
			for(size_t i = 0; i < bufferViewCount; i++)
			{
				uint64_t id;
				avs::BufferView bufferView;
				in.readChunk(id);
				in >>  bufferView;
				mesh.bufferViews[id] = bufferView;
			}

			size_t bufferCount;
			in.readChunk(bufferCount);
			for(size_t i = 0; i < bufferCount; i++)
			{
				uint64_t id;
				avs::GeometryBuffer buffer;
				in.readChunk(id);
				in >>  buffer;
				mesh.buffers[id] = buffer;
			}
			in.readChunk(mesh.inverseBindMatricesAccessorID);
			return in;
		}
	};
	enum class MeshCompressionType:uint8_t
	{
		NONE=0,
		DRACO=1,
		GLB_SEPARATE=2,
		GLB_SCENE=3
	};
	struct CompressedSubMesh
	{
		uint64_t indices_accessor;
		uid material;
		uint32_t first_index;
		uint32_t num_indices;
		std::map<int32_t, AttributeSemantic> attributeSemantics;
		// Raw data buffer of draco (or other) encoded mesh.
		std::vector<uint8_t> buffer;
		bool operator==(const CompressedSubMesh& m) const
		{
			if (indices_accessor != m.indices_accessor)
				return false;
			if (material != m.material)
				return false;
			if (first_index != m.first_index)
				return false;
			if (num_indices != m.num_indices)
				return false;
			if (attributeSemantics != m.attributeSemantics)
				return false;
			if (buffer.size() != m.buffer.size())
				return false;
			if (memcmp(buffer.data(), m.buffer.data(), buffer.size()) != 0)
				return false;
			return true;
		}
		void ResetAccessors(uint64_t subtract)
		{
			indices_accessor=indices_accessor-subtract;
		}
		void GetAccessorRange(uint64_t &lowest,uint64_t &highest) const
		{
			lowest=std::min(lowest,indices_accessor);
			highest=std::max(lowest,indices_accessor);
		}
	};
	struct CompressedMesh
	{
		MeshCompressionType meshCompressionType;
		std::string name;
		std::vector<CompressedSubMesh> subMeshes;
		bool operator==(const CompressedMesh& m) const
		{
			if (meshCompressionType != m.meshCompressionType)
				return false;
			if (name != m.name)
				return false;
			if (subMeshes.size() != m.subMeshes.size())
				return false;
			for (size_t i=0;i<subMeshes.size();i++)
			{
				if (!(subMeshes[i] == m.subMeshes[i]))
					return false;
			}
			return true;
		}
		void ResetAccessors(uint64_t subtract)
		{
			if(subtract>1)
				subtract--;
			else
				return;
			for( auto &subMesh:subMeshes)
			{
				subMesh.ResetAccessors(subtract);
			}
		}
		void GetAccessorRange(uint64_t &lowest,uint64_t &highest) const
		{
			for(const auto &subMesh:subMeshes)
			{
				subMesh.GetAccessorRange(lowest,highest);
			}
		}
		template<typename OutStream> friend OutStream& operator<< (OutStream& out, const CompressedMesh& compressedMesh)
		{
			//Name needs its own line, so spaces can be included.
			out <<  compressedMesh.name;
			out.writeChunk(compressedMesh.meshCompressionType);
			out.writeChunk(compressedMesh.subMeshes.size());
			for (size_t i = 0; i < compressedMesh.subMeshes.size(); i++)
			{
				const auto &subMesh= compressedMesh.subMeshes[i];
				out.writeChunk(subMesh.indices_accessor);
				out<<subMesh.material;
				out.writeChunk(subMesh.first_index);
				out.writeChunk(subMesh.num_indices);
				out.writeChunk(subMesh.attributeSemantics.size());
				for (const auto &a: subMesh.attributeSemantics)
				{
					out.writeChunk(a.first);
					out.writeChunk(a.second);
				}
				out.writeChunk(subMesh.buffer.size());
				out.write((const char*)subMesh.buffer.data(), subMesh.buffer.size());
			}
			return out;
		}

		template<typename InStream>	friend InStream& operator>> (InStream& in, CompressedMesh& compressedMesh)
		{
			in>>compressedMesh.name;
			in.readChunk(compressedMesh.meshCompressionType);
			size_t numSubMeshes = 0;
			in.readChunk(numSubMeshes);
			compressedMesh.subMeshes.resize(numSubMeshes);
			for (size_t i = 0; i < compressedMesh.subMeshes.size(); i++)
			{
				auto& subMesh = compressedMesh.subMeshes[i];
				in.readChunk(subMesh.indices_accessor);
				in>> subMesh.material;
				in.readChunk(subMesh.first_index);
				in.readChunk(subMesh.num_indices);
				size_t numSem=0;
				in.readChunk(numSem);
				for (size_t j=0;j< numSem;j++)
				{
					int32_t s = 0;
					in.readChunk(s);
					in.readChunk(subMesh.attributeSemantics[s]);
				}
				size_t bufferSize = 0;
				in.readChunk(bufferSize);
				subMesh.buffer.resize(bufferSize);
				in.read((char*)subMesh.buffer.data(), bufferSize);
			}
			return in;
		}
	};
	struct MaterialResources
	{
		uid material_uid=0;
		std::vector<uid> texture_uids;
	};

	struct MeshNodeResources
	{
		uid node_uid=0;
		uid mesh_uid=0;
		uid skeletonAssetID=0;
		std::vector<uid> boneIDs;
		std::vector<uid> animationIDs;
		std::vector<MaterialResources> materials;
	};

	struct LightNodeResources
	{
		uid node_uid=0;
		uid shadowmap_uid=0;
	};

	//! This tells the Geometry Source node what it wants from the Geometry Source backend
	//! so the node can acquire the data to be sent for encoding.
	class AVSTREAM_API GeometryRequesterBackendInterface : public UseInternalAllocator
	{
	public:
		//Returns whether this client has the resource.
		virtual bool hasResource(uid resourceID) const = 0;

		//Flags the resource as being sent to this user; the client may send a request response if it didn't actually receive the resource.
		virtual void encodedResource(uid resourceID) = 0;
		//Flags the resources as having been received; prevents the resource being resent after a set amount of time.
		virtual void confirmResource(uid resourceID) = 0;

		//Returns the axes standard used by the client.
		virtual AxesStandard getClientAxesStandard() const = 0;

		//! Returns the rendering features the client supports.
		virtual RenderingFeatures getClientRenderingFeatures() const=0; 
	};
	//! A Geometry decoder backend provides geometry packets to a geometryencoder.
	class AVSTREAM_API GeometryEncoderBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometryEncoderBackendInterface() = default;
		virtual Result encode(uint64_t timestamp) = 0;
		virtual Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) = 0;
		virtual Result unmapOutputBuffer() = 0;
	};

	/*! Structure to pass to GeometryTargetBackendInterface 

	*/
	struct MeshElementCreate
	{
		uint64_t vb_id = 0;
		size_t m_ElementIndex = 0;
		PrimitiveMode primitiveMode= PrimitiveMode::TRIANGLES;

		size_t m_VertexCount = 0;
		size_t m_TangentNormalSize = 0;
		const vec3* m_Vertices = nullptr;
		const vec3* m_Normals = nullptr;
		const vec4* m_Tangents = nullptr;
		const vec2* m_UV0s = nullptr;
		const vec2* m_UV1s = nullptr;
		const vec4* m_Colors = nullptr;
		const vec4* m_Joints = nullptr;
		const vec4* m_Weights = nullptr;
		const uint8_t* m_TangentNormals = nullptr;

		uint64_t ib_id = 0;
		size_t m_IndexCount = 0;
		size_t m_IndexSize = 0;
		const unsigned char* m_Indices = nullptr;
		
		std::shared_ptr<Material> internalMaterial;// not always present.
	};
	struct MeshCreate
	{
		std::string name;
		uid cache_uid = 0;
		uid mesh_uid = 0;
		std::vector<MeshElementCreate> m_MeshElementCreate;
		std::vector<mat4> inverseBindMatrices;
		bool clockwiseFaces=true;
	};
/*!
 * Common mesh decoder backend interface.
 *
 * Mesh backend receives data payloads and will convert them to geometry.
 */
	class AVSTREAM_API GeometryTargetBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometryTargetBackendInterface() = default;
		virtual Result CreateMesh(MeshCreate& meshCreate) = 0;

		virtual void CreateTexture(avs::uid server_uid,uid id, const Texture& texture) = 0;
		virtual void CreateMaterial(avs::uid server_uid,uid id, const Material& material) = 0;
		virtual void CreateNode(avs::uid server_uid,uid id,const Node& node) = 0;
		virtual void CreateSkeleton(avs::uid server_uid,avs::uid id, const avs::Skeleton& skeleton) = 0;
		virtual void CreateAnimation(avs::uid server_uid,avs::uid id, teleport::core::Animation& animation) = 0;
	};


	//! A Geometry decoder backend converts a 
	class AVSTREAM_API GeometryDecoderBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometryDecoderBackendInterface() = default;
		virtual Result decode(avs::uid server_uid,const void* buffer, size_t bufferSizeInBytes, GeometryPayloadType type, GeometryTargetBackendInterface *target,avs::uid uid) = 0;
	};
} // avs
#ifdef _MSC_VER
#pragma pack(pop)
#endif