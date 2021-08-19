// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once
#include <vector>
#include <map>
#include <memory>

#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>

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
	
	enum class PrimitiveMode: uint32_t
	{
		POINTS, LINES, TRIANGLES, LINE_STRIP, TRIANGLE_STRIP
	};
	//! The standard glTF attribute semantics.
	enum class AttributeSemantic : uint32_t
	{
		//Name	Accessor Type(s)	Component Type(s)				Description
		POSITION=0	//"VEC3"	5126 (FLOAT)						XYZ vertex positions
		, NORMAL		//"VEC3"	5126 (FLOAT)						Normalized XYZ vertex normals
		, TANGENT	//"VEC4"	5126 (FLOAT)						XYZW vertex tangents where the w component is a sign value(-1 or +1) indicating handedness of the tangent basis
		, TEXCOORD_0	//"VEC2"	5126 (FLOAT)
					//			5121 (UNSIGNED_BYTE) normalized
					//			5123 (UNSIGNED_SHORT) normalized	UV texture coordinates for the first set
		, TEXCOORD_1	//"VEC2"	5126 (FLOAT)
					//			5121 (UNSIGNED_BYTE) normalized
					//			5123 (UNSIGNED_SHORT) normalized	UV texture coordinates for the second set
		, COLOR_0	//"VEC3"
					//"VEC4"	5126 (FLOAT)
					//			5121 (UNSIGNED_BYTE) normalized
					//			5123 (UNSIGNED_SHORT) normalized	RGB or RGBA vertex color
		, JOINTS_0	//"VEC4"	5121 (UNSIGNED_BYTE)
					//			5123 (UNSIGNED_SHORT)				See Skinned Mesh Attributes
		, WEIGHTS_0	//"VEC4"	5126 (FLOAT)
					//			5121 (UNSIGNED_BYTE) normalized
					//			5123 (UNSIGNED_SHORT) normalized
		, TANGENTNORMALXZ	// VEC2 UNSIGNED_INT					Simul: implements packed tangent-normal xz. Actually two VEC4's of BYTE.
							//		SIGNED_SHORT
		, COUNT		//This is the number of elements in enum class AttributeSemantic{};
					//Must always be the last element in this enum class. 
	};
	struct Attribute
	{
		AttributeSemantic semantic;
		uid accessor;

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
		uid indices_accessor;
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
		size_t byteLength;
		uint8_t* data;

		friend std::wostream& operator<< (std::wostream& out, const GeometryBuffer& buffer)
		{
			out << buffer.byteLength << std::endl;

			for(size_t i = 0; i < buffer.byteLength; i++)
			{
				out.put(out.widen(buffer.data[i]));
			}

			return out;
		}
		
		friend std::wistream& operator>> (std::wistream& in, GeometryBuffer& buffer)
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
		uid buffer;
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
			SCALAR = 0,
			VEC2,
			VEC3,
			VEC4
		};
		enum class ComponentType : uint32_t
		{
			FLOAT
			, DOUBLE
			, HALF
			, UINT
			, USHORT
			, UBYTE
			, INT
			, SHORT
			, BYTE
		};
		DataType type;
		ComponentType componentType;
		size_t count;
		uid bufferView;
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
		vec4 lightmapScaleOffset;
	};

	extern void AVSTREAM_API ConvertTransform(AxesStandard fromStandard, AxesStandard toStandard, Transform &transform);
	extern void AVSTREAM_API ConvertRotation(AxesStandard fromStandard, AxesStandard toStandard, vec4 &rotation);
	extern void AVSTREAM_API ConvertPosition(AxesStandard fromStandard, AxesStandard toStandard, vec3 &position);
	extern void AVSTREAM_API ConvertScale(AxesStandard fromStandard, AxesStandard toStandard, vec3 &scale);
	extern int8_t AVSTREAM_API ConvertAxis(AxesStandard fromStandard, AxesStandard toStandard, int8_t axis);

	struct DataNode
	{
		std::string name;

		Transform transform;
		bool stationary;

		uid parentID;
		std::vector<uid> childrenIDs;

		NodeDataType data_type;
		NodeDataSubtype data_subtype;
		uid data_uid;

		//MESH
		std::vector<uid> materials;
		//SKINNED MESH
		uid skinID;
		std::vector<uid> animations;

		// e.g. lightmap offset and multiplier.
		avs::NodeRenderState renderState;

		//LIGHT
		vec4 lightColour;
		float lightRadius;
		vec3 lightDirection; // Unchanging rotation that orients the light's shadowspace so that it shines on the Z axis with X and Y for shadowmap.
		uint8_t lightType;
		float lightRange;	// Maximum distance the light is effective at, in metres.
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
		if (t == Accessor::ComponentType::UBYTE)
			return 2;
		if (t == Accessor::ComponentType::USHORT)
			return 2;
		if (t == Accessor::ComponentType::DOUBLE)
			return 8;
		return 4;
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
		std::vector<uid> jointIDs;
		Transform skinTransform;

		static Skin convertToStandard(const Skin& skin, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
		{
			avs::Skin convertedSkin;
			convertedSkin.name = skin.name;
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
		std::unordered_map<uid, Accessor> accessors;
		std::map<uid, BufferView> bufferViews;
		std::map<uid, GeometryBuffer> buffers;

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

			size_t primitiveArrayAmount;
			in >> primitiveArrayAmount;
			mesh.primitiveArrays.resize(primitiveArrayAmount);
			for(size_t i = 0; i < primitiveArrayAmount; i++)
			{
				in >> mesh.primitiveArrays[i];
			}

			size_t accessorAmount;
			in >> accessorAmount;
			for(size_t i = 0; i < accessorAmount; i++)
			{
				avs::uid id;
				avs::Accessor accessor;

				in >> id >> accessor;
				mesh.accessors[id] = accessor;
			}

			size_t bufferViewAmount;
			in >> bufferViewAmount;
			for(size_t i = 0; i < bufferViewAmount; i++)
			{
				avs::uid id;
				avs::BufferView bufferView;

				in >> id >> bufferView;
				mesh.bufferViews[id] = bufferView;
			}

			size_t bufferAmount;
			in >> bufferAmount;
			for(size_t i = 0; i < bufferAmount; i++)
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
		uid indices_accessor;
		uid material;
		uint32_t first_index;
		uint32_t num_indices;
		std::map<int32_t, AttributeSemantic> attributeSemantics;
		// Raw data buffer of draco (or other) encoded mesh.
		std::vector<uint8_t> buffer;
		//uint8_t subMeshAttributeIndex=0;		// which attribute is the subMesh index if any.
	};
	struct CompressedMesh
	{
		MeshCompressionType meshCompressionType;
		std::string name;
		std::vector<CompressedSubMesh> subMeshes;
		template<typename OutStream> friend OutStream& operator<< (OutStream& out, const CompressedMesh& compressedMesh)
		{
			//Name needs its own line, so spaces can be included.
			out << std::wstring{ compressedMesh.name.begin(), compressedMesh.name.end() } << std::endl;

			out << (uint32_t)compressedMesh.meshCompressionType;
			//out << " " << (uint32_t)compressedMesh.subMeshAttributeIndex;
			out << " " << compressedMesh.subMeshes.size(); 
			for (size_t i = 0; i < compressedMesh.subMeshes.size(); i++)
			{
				out << " " << compressedMesh.subMeshes[i].indices_accessor;
				out << " " << compressedMesh.subMeshes[i].material;
				out << " " << compressedMesh.subMeshes[i].first_index;
				out << " " << compressedMesh.subMeshes[i].num_indices;
			}
			out << " " << compressedMesh.attributeSemantics.size() << std::endl;
			for (const auto &a: compressedMesh.attributeSemantics)
			{
				out << " " << a.first;
				out << " " << (int32_t)a.second;
			}
			out << " " << compressedMesh.buffer.size() << std::endl;

			size_t num_c= compressedMesh.buffer.size() ;
			// have to do this because of dumb decision to use wchar_t instead of bytes. Change this!
			for(size_t i=0;i<num_c;i++)
			{
				out.put(out.widen(compressedMesh.buffer[i]));
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
			uint32_t subMesh = 0;
			in >> subMesh;
			compressedMesh.subMeshAttributeIndex= (uint8_t)subMesh;
			size_t numSubMeshes=0;
			in >> numSubMeshes;
			compressedMesh.subMeshes.resize(numSubMeshes);
			for (size_t i = 0; i < compressedMesh.subMeshes.size(); i++)
			{
				in >> compressedMesh.subMeshes[i].indices_accessor;
				in >> compressedMesh.subMeshes[i].material;
				in >> compressedMesh.subMeshes[i].first_index;
				in >> compressedMesh.subMeshes[i].num_indices;
			}
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
				compressedMesh.attributeSemantics[attr]= (AttributeSemantic)semantic;
			}
			size_t bufferSize=0;
			in >> bufferSize; 
			//Discard new line.
			in.get();
			compressedMesh.buffer.resize(bufferSize);
			size_t num_c = compressedMesh.buffer.size();
			// have to do this because of dumb decision to use wchar_t instead of bytes. Change this!
			for (size_t i = 0; i < num_c; i++)
			{
				// I mean honestly:
				compressedMesh.buffer[i]= in.narrow(in.get(), '\000');
			}
			return in;
		}
	};
	struct MaterialResources
	{
		uid material_uid;
		std::vector<uid> texture_uids;
	};

	struct MeshNodeResources
	{
		uid node_uid;
		uid mesh_uid;
		uid skinID;
		std::vector<uid> jointIDs;
		std::vector<uid> animationIDs;
		std::vector<MaterialResources> materials;
	};

	struct LightNodeResources
	{
		uid node_uid;
		uid shadowmap_uid=0;
	};

	//! Common mesh backend interface.
	//! Mesh backend abstracts mesh definition.
	//! We aim to follow the glTF format as a starting point https://www.khronos.org/gltf/
	class AVSTREAM_API GeometrySourceBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometrySourceBackendInterface() = default;

		//! A Node has a transform, and MAY contain an instance of a mesh.
		virtual std::vector<uid> getNodeIDs() const = 0;
		//! If it exists, get the name of the node, otherwise an empty string.
		virtual const char* getNodeName(avs::uid nodeID) const =0;
		//Get node with passed ID.
		//	nodeID : Indentifier of the node.
		//Returns the node if successfully found, otherwise nullptr.
		virtual DataNode* getNode(uid nodeID) = 0;
		virtual const DataNode* getNode(uid nodeID) const = 0;
		//! Nodes make up the hierarchy of the scene
		virtual const std::map<uid, DataNode>& getNodes() const = 0;

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
										,std::vector<avs::uid>& genericTextureUids) const = 0;

		//Returns the axes standard used by the client.
		virtual AxesStandard getClientAxesStandard() const = 0;
	};
	//! A Geometry decoder backend converts a 
	class AVSTREAM_API GeometryEncoderBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometryEncoderBackendInterface() = default;
		virtual Result encode(uint32_t timestamp, GeometrySourceBackendInterface* target, GeometryRequesterBackendInterface* requester) = 0;
		virtual Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) = 0;
		virtual Result unmapOutputBuffer() = 0;
	};

	/*! Structure to pass to GeometryTargetBackendInterface 

	*/
	struct MeshElementCreate
	{
		uid vb_uid = 0;
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

		uid ib_uid = 0;
		size_t m_IndexCount = 0;
		size_t m_IndexSize = 0;
		const unsigned char* m_Indices = nullptr;
	};
	struct MeshCreate
	{
		std::string name;

		uid mesh_uid = 0;
		size_t m_NumElements = 0;
		MeshElementCreate m_MeshElementCreate[8];
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
		virtual Result Assemble(MeshCreate& meshCreate) = 0;

		virtual void CreateTexture(uid id, const Texture& texture) = 0;
		virtual void CreateMaterial(uid id, const Material& material) = 0;
		virtual void CreateNode(uid id, DataNode& node) = 0;
		virtual void CreateSkin(avs::uid id, avs::Skin& skin) = 0;
		virtual void CreateAnimation(avs::uid id, avs::Animation& animation) = 0;
	};

	//! A Geometry decoder backend converts a 
	class AVSTREAM_API GeometryDecoderBackendInterface : public UseInternalAllocator
	{
	public:
		virtual ~GeometryDecoderBackendInterface() = default;
		virtual Result decode(const void* buffer, size_t bufferSizeInBytes, GeometryPayloadType type, GeometryTargetBackendInterface *target) = 0;
	};

	class GeometryParserInterface
	{
	public:
		virtual ~GeometryParserInterface() = default;
		virtual GeometryPayloadType classify(const uint8_t* buffer, size_t bufferSize, size_t& dataOffset) const = 0;
		static constexpr size_t HeaderSize = 2;
	};
} // avs