// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once
#include <vector>
#include <map>
#include <memory>
#include <set>

#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>
//#include <TeleportCore/CommonNetworking.h>

#include "material_interface.hpp"

namespace avs
{
	struct Animation;

	template<typename T> struct Vec2
	{
		T x, y;
	};
	template<typename T> struct Vec3
	{
		T x, y, z;
	};
	template<typename T> struct Vec4
	{
		T x, y, z, w;
	};
	
	enum class PrimitiveMode : uint32_t
	{
		POINTS, LINES, TRIANGLES, LINE_STRIP, TRIANGLE_STRIP
	};
	//! The standard glTF attribute semantics.
	enum class AttributeSemantic : uint32_t
	{
		//Name		Accessor Type(s)	Component Type(s)					Description
		POSITION=0,			//"VEC3"	5126 (FLOAT)						XYZ vertex positions
		NORMAL,				//"VEC3"	5126 (FLOAT)						Normalized XYZ vertex normals
		TANGENT,			//"VEC4"	5126 (FLOAT)						XYZW vertex tangents where the w component is a sign value(-1 or +1) indicating handedness of the tangent basis
		TEXCOORD_0,			//"VEC2"	5126 (FLOAT)
							//			5121 (UNSIGNED_BYTE) normalized
							//			5123 (UNSIGNED_SHORT) normalized	UV texture coordinates for the first set
		TEXCOORD_1,			//"VEC2"	5126 (FLOAT)
							//			5121 (UNSIGNED_BYTE) normalized
							//			5123 (UNSIGNED_SHORT) normalized	UV texture coordinates for the second set
		COLOR_0,			//"VEC3"
							//"VEC4"	5126 (FLOAT)
							//			5121 (UNSIGNED_BYTE) normalized
							//			5123 (UNSIGNED_SHORT) normalized	RGB or RGBA vertex color
		JOINTS_0,			//"VEC4"	5121 (UNSIGNED_BYTE)
							//			5123 (UNSIGNED_SHORT)				See Skinned Mesh Attributes
		WEIGHTS_0,			//"VEC4"	5126 (FLOAT)
							//			5121 (UNSIGNED_BYTE) normalized
							//			5123 (UNSIGNED_SHORT) normalized
		TANGENTNORMALXZ,	//"VEC2"		 (UNSIGNED_INT)					Simul: implements packed tangent-normal xz. Actually two VEC4's of BYTE.
							//				 (SIGNED_SHORT)
		COUNT				//This is the number of elements in enum class AttributeSemantic{};
							//Must always be the last element in this enum class. 
	};
	struct Attribute
	{
		AttributeSemantic semantic;
		uint64_t accessor;

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Attribute& attribute)
		{
			return out << static_cast<uint32_t>(attribute.semantic) << " " << attribute.accessor;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, Attribute& attribute)
		{
			uint32_t semantic;

			in >> semantic >> attribute.accessor;
			attribute.semantic = static_cast<AttributeSemantic>(semantic);

			return in;
		}
	};
	struct PrimitiveArray
	{
		size_t attributeCount;
		Attribute *attributes;
		uint64_t indices_accessor;
		uid material;
		PrimitiveMode primitiveMode;

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const PrimitiveArray& primitiveArray)
		{
			out << primitiveArray.attributeCount;
			for(size_t i = 0; i < primitiveArray.attributeCount; i++)
			{
				out << " " << primitiveArray.attributes[i];
			}

			return out << " " << primitiveArray.indices_accessor
				<< " " << primitiveArray.material
				<< " " << static_cast<uint32_t>(primitiveArray.primitiveMode);
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, PrimitiveArray& primitiveArray)
		{
			in >> primitiveArray.attributeCount;
			primitiveArray.attributes = new Attribute[primitiveArray.attributeCount];
			for(size_t i = 0; i < primitiveArray.attributeCount; i++)
			{
				in >> primitiveArray.attributes[i];
			}

			in >> primitiveArray.indices_accessor >> primitiveArray.material;

			uint32_t primitiveMode;
			in >> primitiveMode;
			primitiveArray.primitiveMode = static_cast<PrimitiveMode>(primitiveMode);

			return in;
		}
	};

	//! A buffer of arbitrary binary data, which should not be freed.
	struct GeometryBuffer
	{
		size_t byteLength=0;
		uint8_t* data=nullptr;

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const GeometryBuffer& buffer)
		{
			out << buffer.byteLength << std::endl;

			for(size_t i = 0; i < buffer.byteLength; i++)
			{
				out.put(out.widen(buffer.data[i]));
			}

			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, GeometryBuffer& buffer)
		{
			in >> buffer.byteLength;

			{
				//Discard new line.
				in.get();

				buffer.data = new uint8_t[buffer.byteLength];
				for(size_t i = 0; i < buffer.byteLength; i++)
				{
					buffer.data[i] = in.narrow(in.get(), '\000');
				}
			}

			return in;
		}
	};

	//! A view into a buffer. Could either be a contiguous subset of the data, or a stride-view skipping elements.
	struct BufferView
	{
		uint64_t buffer;
		size_t byteOffset;
		size_t byteLength;
		size_t byteStride;

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const BufferView& bufferView)
		{
			return out << bufferView.buffer << " " << bufferView.byteOffset << " " << bufferView.byteLength << " " << bufferView.byteStride;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, BufferView& bufferView)
		{
			return in >> bufferView.buffer >> bufferView.byteOffset >> bufferView.byteLength >> bufferView.byteStride;
		}
	};

	struct Accessor
	{
		enum class DataType : uint32_t
		{
			SCALAR = 1,
			VEC2,
			VEC3,
			VEC4
		};
		enum class ComponentType : uint32_t
		{
			FLOAT=0,
			DOUBLE,
			HALF,
			UINT,
			USHORT,
			UBYTE,
			INT,
			SHORT,
			BYTE,
		};
		DataType type;
		ComponentType componentType;
		size_t count;
		uint64_t bufferView;
		size_t byteOffset;

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Accessor& accessor)
		{
			return out << static_cast<uint32_t>(accessor.type)
				<< " " << static_cast<uint32_t>(accessor.componentType)
				<< " " << accessor.count
				<< " " << accessor.bufferView
				<< " " << accessor.byteOffset;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, Accessor& accessor)
		{
			uint32_t type, componentType;

			in >> type >> componentType >> accessor.count >> accessor.bufferView >> accessor.byteOffset;
			accessor.type = static_cast<DataType>(type);
			accessor.componentType = static_cast<ComponentType>(componentType);

			return in;
		}
	};

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
	struct Transform
	{
		vec3 position = { 0, 0, 0 };
		vec4 rotation = { 0, 0, 0, 1 };
		vec3 scale = { 1, 1, 1 };
	};

	struct NodeRenderState
	{
		vec4 lightmapScaleOffset={0,0,0,0};
		uid globalIlluminationUid = 0;
	};

	extern void AVSTREAM_API ConvertTransform(AxesStandard fromStandard, AxesStandard toStandard, Transform &transform);
	extern void AVSTREAM_API ConvertRotation(AxesStandard fromStandard, AxesStandard toStandard, vec4 &rotation);
	extern void AVSTREAM_API ConvertPosition(AxesStandard fromStandard, AxesStandard toStandard, vec3 &position);
	extern void AVSTREAM_API ConvertScale(AxesStandard fromStandard, AxesStandard toStandard, vec3 &scale);
	extern int8_t AVSTREAM_API ConvertAxis(AxesStandard fromStandard, AxesStandard toStandard, int8_t axis);

	struct Node
	{
		std::string name;

		Transform localTransform;
		Transform globalTransform;

		bool stationary=false;

		uid holder_client_id=0;

		int32_t priority=0;

		uid parentID=0;
		std::vector<uid> childrenIDs;

		// The following should be separated out into node components:
		NodeDataType data_type=NodeDataType::None;
		uid data_uid=0;

		//MESH
		std::vector<uid> materials;

		//SKINNED MESH
		uid skinID=0;
		std::vector<uid> animations;

		// e.g. lightmap offset and multiplier.
		NodeRenderState renderState;

		//LIGHT
		vec4 lightColour={0,0,0,0};
		float lightRadius=0.f;
		vec3 lightDirection={0,0,1.0f};	// Unchanging rotation that orients the light's shadowspace so that it shines on the Z axis with X and Y for shadowmap.
		uint8_t lightType=0;
		float lightRange=0.f;			//! Maximum distance the light is effective at, in metres.

	};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

	inline size_t GetComponentSize(Accessor::ComponentType t)
	{
		switch (t)
		{
		case Accessor::ComponentType::BYTE:
		case Accessor::ComponentType::UBYTE:
			return 1;
		case Accessor::ComponentType::HALF:
		case Accessor::ComponentType::SHORT:
		case Accessor::ComponentType::USHORT:
			return 2;
		case Accessor::ComponentType::FLOAT:
		case Accessor::ComponentType::INT:
		case Accessor::ComponentType::UINT:
			return 4;
		case Accessor::ComponentType::DOUBLE:
			return 8;
		default:
			return 1;
		};
	}

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
		case Accessor::DataType::VEC4:
		default:
			return 4;
		};
	}

	struct Skin
	{
		std::string name;
		std::vector<Mat4x4> inverseBindMatrices;
		std::vector<uid> boneIDs;
		// of which a subset is:
		std::vector<uid> jointIDs;
		Transform skinTransform;
		// New method, bones are built into the skin:
		std::vector<Transform> boneTransforms;
		std::vector<uint16_t> parentIndices;
		std::vector<uint16_t> jointIndices;
		std::vector<std::string> boneNames;

		static Skin convertToStandard(const Skin& skin, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
		{
			avs::Skin convertedSkin;
			convertedSkin.name = skin.name;
			convertedSkin.boneIDs = skin.boneIDs;
			convertedSkin.jointIDs = skin.jointIDs;

			for(const Mat4x4& matrix : skin.inverseBindMatrices)
			{
				convertedSkin.inverseBindMatrices.push_back(Mat4x4::convertToStandard(matrix, sourceStandard, targetStandard));
			}

			convertedSkin.skinTransform = skin.skinTransform;
			avs::ConvertTransform(sourceStandard, targetStandard, convertedSkin.skinTransform);

			return convertedSkin;
		}
	};

	struct Mesh
	{
		std::string name;

		std::vector<PrimitiveArray> primitiveArrays;
		std::unordered_map<uint64_t, Accessor> accessors;
		std::map<uint64_t, BufferView> bufferViews;
		std::map<uint64_t, GeometryBuffer> buffers;
		void ResetAccessors(uint64_t subtract)
		{
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
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Mesh& mesh)
		{
			//Name needs its own line, so spaces can be included.
			out << std::wstring{mesh.name.begin(), mesh.name.end()} << std::endl;

			out << mesh.primitiveArrays.size();
			for(size_t i = 0; i < mesh.primitiveArrays.size(); i++)
			{
				out << " " << mesh.primitiveArrays[i];
			}

			out << " " << mesh.accessors.size();
			for(auto& accessorPair : mesh.accessors)
			{
				out << " " << accessorPair.first << " " << accessorPair.second;
			}

			out << " " << mesh.bufferViews.size();
			for(auto& bufferViewPair : mesh.bufferViews)
			{
				out << " " << bufferViewPair.first << " " << bufferViewPair.second;
			}

			out << " " << mesh.buffers.size();
			for(auto& bufferPair : mesh.buffers)
			{
				out << " " << bufferPair.first << " " << bufferPair.second;
			}
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, Mesh& mesh)
		{
			//Step past new line that may be next in buffer.
			if(in.peek() == '\n')
				in.get();

			//Read name with spaces included.
			std::wstring wideName;
			std::getline(in, wideName);

			mesh.name = convertToByteString(wideName);

			size_t primitiveArrayCount;
			in >> primitiveArrayCount;
			mesh.primitiveArrays.resize(primitiveArrayCount);
			for(size_t i = 0; i < primitiveArrayCount; i++)
			{
				in >> mesh.primitiveArrays[i];
			}

			size_t accessorCount;
			in >> accessorCount;
			for(size_t i = 0; i < accessorCount; i++)
			{
				avs::uid id;
				avs::Accessor accessor;

				in >> id >> accessor;
				mesh.accessors[id] = accessor;
			}

			size_t bufferViewCount;
			in >> bufferViewCount;
			for(size_t i = 0; i < bufferViewCount; i++)
			{
				avs::uid id;
				avs::BufferView bufferView;

				in >> id >> bufferView;
				mesh.bufferViews[id] = bufferView;
			}

			size_t bufferCount;
			in >> bufferCount;
			for(size_t i = 0; i < bufferCount; i++)
			{
				avs::uid id;
				avs::GeometryBuffer buffer;

				in >> id >> buffer;
				mesh.buffers[id] = buffer;
			}

			return in;
		}
	};
	enum class MeshCompressionType:uint8_t
	{
		NONE=0,
		DRACO=1
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
		//uint8_t subMeshAttributeIndex=0;		// which attribute is the subMesh index if any.
		
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
		
		void ResetAccessors(uint64_t subtract)
		{
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
			out << std::wstring{ compressedMesh.name.begin(), compressedMesh.name.end() } << std::endl;

			out << (uint32_t)compressedMesh.meshCompressionType;
			//out << " " << (uint32_t)compressedMesh.subMeshAttributeIndex;
			out << " " << compressedMesh.subMeshes.size(); 
			for (size_t i = 0; i < compressedMesh.subMeshes.size(); i++)
			{
				const auto &subMesh= compressedMesh.subMeshes[i];
				out << " " << subMesh.indices_accessor;
				out << " " << subMesh.material;
				out << " " << subMesh.first_index;
				out << " " << subMesh.num_indices;
				out << " " << subMesh.attributeSemantics.size() << std::endl;
				for (const auto &a: subMesh.attributeSemantics)
				{
					out << " " << a.first;
					out << " " << (int32_t)a.second;
				}
				out << " " << subMesh.buffer.size() << std::endl;

				size_t num_c= subMesh.buffer.size() ;
				// have to do this because of dumb decision to use wchar_t instead of bytes. Change this!
				for(size_t i=0;i<num_c;i++)
				{
					out.put(out.widen(subMesh.buffer[i]));
				}
			}
			return out;
		}

		template<typename InStream>	friend InStream& operator>> (InStream& in, CompressedMesh& compressedMesh)
		{
			//Step past new line that may be next in buffer.
			if (in.peek() == '\n')
				in.get();
			//Read name with spaces included.
			std::wstring wideName;
			std::getline(in, wideName);
			compressedMesh.name = convertToByteString(wideName);
			uint32_t type=0;
			in >> type;
			compressedMesh.meshCompressionType=(MeshCompressionType)type;
			//uint32_t subMesh = 0;
			//in >> subMesh;
			//compressedMesh.subMeshAttributeIndex= (uint8_t)subMesh;
			size_t numSubMeshes=0;
			in >> numSubMeshes;
			compressedMesh.subMeshes.resize(numSubMeshes);
			for (size_t i = 0; i < compressedMesh.subMeshes.size(); i++)
			{
				auto& subMesh = compressedMesh.subMeshes[i];
				in >> subMesh.indices_accessor;
				in >> subMesh.material;
				in >> subMesh.first_index;
				in >> subMesh.num_indices;
				size_t numAttrSem = 0;
				in >> numAttrSem ;
				//Discard new line.
				in.get();
				for (size_t i=0;i< numAttrSem;i++)
				{
					int32_t attr= 0;
					int32_t semantic=0;
					in >> attr;
					in >> semantic;
					subMesh.attributeSemantics[attr]= (AttributeSemantic)semantic;
				}
				size_t bufferSize=0;
				in >> bufferSize; 
				//Discard new line.
				in.get();
				subMesh.buffer.resize(bufferSize);
				size_t num_c = subMesh.buffer.size();
				// have to do this because of dumb decision to use wchar_t instead of bytes. Change this!
				for (size_t i = 0; i < num_c; i++)
				{
					// I mean honestly:
					subMesh.buffer[i]= in.narrow(in.get(), '\000');
				}
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
		uid skinID=0;
		std::vector<uid> boneIDs;
		std::vector<uid> animationIDs;
		std::vector<MaterialResources> materials;
	};

	struct LightNodeResources
	{
		uid node_uid=0;
		uid shadowmap_uid=0;
	};

	//! Common mesh backend interface.
	//! Mesh backend abstracts mesh definition.
	//! We aim to follow the glTF format as a starting point https://www.khronos.org/gltf/
	/*class AVSTREAM_API GeometrySourceBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometrySourceBackendInterface() = default;

		//Get IDs of all meshes stored.
		virtual std::vector<uid> getMeshIDs() const = 0;
		virtual const avs::CompressedMesh* getCompressedMesh(avs::uid meshID, avs::AxesStandard standard) const =0;
		//Get mesh with passed ID.
		//	meshID : Indentifier of the mesh.
		//Returns the mesh if successfully found, otherwise nullptr.
		virtual Mesh* getMesh(uid meshID, avs::AxesStandard standard) = 0;
		virtual const Mesh* getMesh(uid meshID, avs::AxesStandard standard) const = 0;

		virtual avs::Skin* getSkin(avs::uid skinID, avs::AxesStandard standard) = 0;
		virtual const avs::Skin* getSkin(avs::uid skinID, avs::AxesStandard standard) const = 0;

		virtual avs::Animation* getAnimation(avs::uid id, avs::AxesStandard standard) = 0;
		virtual const avs::Animation* getAnimation(avs::uid id, avs::AxesStandard standard) const = 0;

		//Get IDs of all textures stored in this geometry source.
		virtual std::vector<uid> getTextureIDs() const = 0;
		//Get texture with passed ID.
		//	textureID : Indentifier of the texture.
		//Returns the texture if successfully found, otherwise nullptr.
		virtual Texture* getTexture(uid textureID) = 0;
		virtual const Texture* getTexture(uid textureID) const = 0;

		//Get IDs of all materials stored in this geometry source.
		virtual std::vector<uid> getMaterialIDs() const = 0;
		//Get material with passed ID.
		//	materialID : Indentifier of the material.
		//Returns the material if successfully found, otherwise nullptr.
		virtual Material* getMaterial(uid materialID) = 0;
		virtual const Material* getMaterial(uid materialID) const = 0;

		//Get IDs of all shadow maps stored in this geometry source.
		virtual std::vector<uid> getShadowMapIDs() const = 0;
		//Get shadow map with passed ID.
		//	shadowID : Indentifier of the shadow map.
		//Returns the shadow map if successfully found, otherwise nullptr.
		virtual Texture* getShadowMap(uid shadowID) = 0;
		virtual const Texture* getShadowMap(uid shadowID) const = 0;
	};*/
	//! This tells the Geometry Source node what it wants from the Geometry Source backend
	//! so the node can acquire the data to be sent for encoding.
	class AVSTREAM_API GeometryRequesterBackendInterface : public UseInternalAllocator
	{
	public:
		//Returns whether this client has the resource.
		virtual bool hasResource(uid resourceID) const = 0;

		//Flags the resource as being sent to this user; the client may send a request response if it didn't actually receive the resource.
		virtual void encodedResource(uid resourceID) = 0;
		//Flags the resource as needing to be sent to this user.
		virtual void requestResource(uid resourceID) = 0;
		//Flags the resources as having been received; prevents the resource being resent after a set amount of time.
		virtual void confirmResource(uid resourceID) = 0;

		//Get resources to stream on a per node basis.
		//	outNodeIDs : IDs of nodes to stream with no dependencies on sub-resources.
		//	outMeshResources : Resources to stream for mesh nodes.
		//	outLightResources : Resources to stream for light nodes.
		//	genericTextureUids : Textures that are not specific members of materials, e.g. lightmaps.
		virtual void getResourcesToStream(std::vector<avs::uid>& outNodeIDs
										,std::vector<MeshNodeResources>& outMeshResources
										,std::vector<LightNodeResources>& outLightResources
										,std::set<avs::uid>& genericTextureUids, int32_t minimumPriority) const = 0;

		//Returns the axes standard used by the client.
		virtual AxesStandard getClientAxesStandard() const = 0;

		//! Returns the rendering features the client supports.
		virtual RenderingFeatures getClientRenderingFeatures() const=0; 
	};
	//! A Geometry decoder backend converts a 
	class AVSTREAM_API GeometryEncoderBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometryEncoderBackendInterface() = default;
		virtual Result encode(uint64_t timestamp, GeometryRequesterBackendInterface* requester) = 0;
		virtual Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) = 0;
		virtual Result unmapOutputBuffer() = 0;
		virtual void setMinimumPriority(int32_t) =0;
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
	};
	struct MeshCreate
	{
		std::string name;

		uid mesh_uid = 0;
		std::vector<MeshElementCreate> m_MeshElementCreate;
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

		virtual void CreateTexture(uid id, const Texture& texture) = 0;
		virtual void CreateMaterial(uid id, const Material& material) = 0;
		virtual void CreateNode(uid id, Node& node) = 0;
		virtual void CreateSkin(avs::uid id, avs::Skin& skin) = 0;
		virtual void CreateAnimation(avs::uid id, avs::Animation& animation) = 0;
	};

	class AVSTREAM_API GeometryCacheBackendInterface : public UseInternalAllocator
	{
	public:
		virtual std::vector<avs::uid> GetCompletedNodes() const = 0;
		virtual std::vector<avs::uid> GetReceivedResources() const = 0;
		virtual std::vector<avs::uid> GetResourceRequests() const = 0;
		virtual void ClearCompletedNodes() = 0;
		virtual void ClearReceivedResources() = 0;
		virtual void ClearResourceRequests() = 0;
	};

	//! A Geometry decoder backend converts a 
	class AVSTREAM_API GeometryDecoderBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometryDecoderBackendInterface() = default;
		virtual Result decode(const void* buffer, size_t bufferSizeInBytes, GeometryPayloadType type, GeometryTargetBackendInterface *target) = 0;
	};
} // avs