// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once
#include <libavstream/common_packing.h>
#include <libavstream/common_exports.h>
#include <libavstream/common_maths.h>

#pragma pack(push, 1)
namespace avs
{
	struct AVS_PACKED NodeRenderState
	{
		vec4 lightmapScaleOffset={0,0,0,0};
		uid globalIlluminationUid = 0;
		uint8_t lightmapTextureCoordinate=0;
	} AVS_PACKED;
	static_assert (sizeof(NodeRenderState) == 25, "avs::NodeRenderState size is not correct");

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
		TEXCOORD_2,
		TEXCOORD_3,
		TEXCOORD_4,
		TEXCOORD_5,
		TEXCOORD_6,
		COLOR_0,			//"VEC3"
							//"VEC4"	5126 (FLOAT)
							//			5121 (UNSIGNED_BYTE) normalized
							//			5123 (UNSIGNED_SHORT) normalized	RGB or RGBA vertex color
		JOINTS_0,			//"VEC4"	5121 (UNSIGNED_BYTE)
							//			5123 (UNSIGNED_SHORT)				See Skeletonned Mesh Attributes
		WEIGHTS_0,			//"VEC4"	5126 (FLOAT)
							//			5121 (UNSIGNED_BYTE) normalized
							//			5123 (UNSIGNED_SHORT) normalized
		TANGENTNORMALXZ,	//"VEC2"		 (UNSIGNED_INT)					Simul: implements packed tangent-normal xz. Actually two VEC4's of BYTE.
							//				 (SIGNED_SHORT)
		COUNT				//This is the number of elements in enum class AttributeSemantic{};
							//Must always be the last element in this enum class. 
	};
	inline const char* stringOf(avs::AttributeSemantic s)
	{
		switch(s)
		{
		case avs::AttributeSemantic::POSITION		:				return "POSITION";
		case avs::AttributeSemantic::NORMAL			:				return "NORMAL";
		case avs::AttributeSemantic::TANGENT		:				return "TANGENT";
		case avs::AttributeSemantic::TEXCOORD_0		:				return "TEXCOORD_0";
		case avs::AttributeSemantic::TEXCOORD_1		:				return "TEXCOORD_1";
		case avs::AttributeSemantic::TEXCOORD_2		:				return "TEXCOORD_2";
		case avs::AttributeSemantic::TEXCOORD_3		:				return "TEXCOORD_3";
		case avs::AttributeSemantic::TEXCOORD_4		:				return "TEXCOORD_4";
		case avs::AttributeSemantic::TEXCOORD_5		:				return "TEXCOORD_5";
		case avs::AttributeSemantic::TEXCOORD_6		:				return "TEXCOORD_6";
		case avs::AttributeSemantic::COLOR_0		:				return "COLOR_0";
		case avs::AttributeSemantic::JOINTS_0		:				return "JOINTS_0";
		case avs::AttributeSemantic::WEIGHTS_0		:				return "WEIGHTS_0";
		case avs::AttributeSemantic::TANGENTNORMALXZ:				return "TANGENTNORMALXZ";
		case avs::AttributeSemantic::COUNT			:				return "COUNT";
		default:							return "INVALID";
		};
	}
	struct Attribute
	{
		AttributeSemantic semantic;
		uint64_t accessor;
		bool operator==(const Attribute& a) const
		{
			if (semantic != a.semantic)
				return false;
			if (accessor != a.accessor)
				return false;
			return true;
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Attribute& attribute)
		{
			out.writeChunk(attribute.semantic);
			out.writeChunk(attribute.accessor);
			return out ;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, Attribute& attribute)
		{
			in.readChunk(attribute.semantic);
			in.readChunk(attribute.accessor);
			return in;
		}
	};
	inline bool CompareMemory(const void* a, const void* b, size_t s)
	{
		uint8_t* A = (uint8_t*)a;
		uint8_t* B = (uint8_t*)b;
		for (size_t i = 0; i < s; i++)
		{
			if (A[i] != B[i])
			{
				// failed at i.
				return false;
			}
		}
		return true;
	}
	struct PrimitiveArray
	{
		size_t attributeCount;
		Attribute *attributes;
		uint64_t indices_accessor;
		uid material;
		PrimitiveMode primitiveMode;
		bool operator==(const PrimitiveArray& p) const
		{
			if (attributeCount != p.attributeCount)
				return false;
			if (indices_accessor != p.indices_accessor)
				return false;
			if (material != p.material)
				return false;
			if (primitiveMode != p.primitiveMode)
				return false;
			for (size_t i = 0; i < attributeCount; i++)
			{
				if (!(attributes[i] == p.attributes[i]))
					return false;
			}
			return true;
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const PrimitiveArray& primitiveArray)
		{
			out.writeChunk(primitiveArray.attributeCount);
			for(size_t i = 0; i < primitiveArray.attributeCount; i++)
			{
				out <<  primitiveArray.attributes[i];
			}

			out.writeChunk(primitiveArray.indices_accessor);
			out << primitiveArray.material;
			out.writeChunk(primitiveArray.primitiveMode);
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, PrimitiveArray& primitiveArray)
		{
			in.readChunk(primitiveArray.attributeCount);
			primitiveArray.attributes = new Attribute[primitiveArray.attributeCount];
			for(size_t i = 0; i < primitiveArray.attributeCount; i++)
			{
				in >> primitiveArray.attributes[i];
			}
			in.readChunk(primitiveArray.indices_accessor);
			in >> primitiveArray.material;
			in.readChunk(primitiveArray.primitiveMode);
			return in;
		}
	};

	//! A buffer of arbitrary binary data, which should not be freed.
	struct GeometryBuffer
	{
		size_t byteLength=0;
		uint8_t* data=nullptr;

		bool operator==(const GeometryBuffer& b) const
		{
			if (byteLength != b.byteLength)
				return false;
			if (memcmp(data, b.data, byteLength) != 0)
				return false;
			return true;
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const GeometryBuffer& buffer)
		{
			out.writeChunk(buffer.byteLength);
			out.write((const char*)buffer.data, buffer.byteLength);
			return out;
		}
		
		template<typename InStream>
		friend InStream& operator>> (InStream& in, GeometryBuffer& buffer)
		{
			in.readChunk(buffer.byteLength);
			buffer.data = new uint8_t[buffer.byteLength];
			in.read((char*)buffer.data, buffer.byteLength);

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
		bool operator==(const BufferView& b) const
		{
			if (memcmp(this, &b, sizeof(BufferView)) != 0)
				return false;
			return true;
		}
		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const BufferView& bufferView)
		{
			out.writeChunk(bufferView);
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, BufferView& bufferView)
		{
			in.readChunk(bufferView);
			return in ;
		}
	};

	struct Accessor
	{
		enum class DataType : uint32_t
		{
			SCALAR = 1,
			VEC2,
			VEC3,
			VEC4,
			MAT4
		};
		DataType type;
		ComponentType componentType;
		size_t count;
		uint64_t bufferView;
		size_t byteOffset;
		bool operator==(const Accessor& a) const
		{
			if (memcmp(this, &a, sizeof(Accessor)) != 0)
				return false;
			return true;
		}

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const Accessor& accessor)
		{
			out.writeChunk(accessor.type);
			out.writeChunk(accessor.componentType);
			out.writeChunk(accessor.count);
			out.writeChunk(accessor.bufferView);
			out.writeChunk(accessor.byteOffset);
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, Accessor& accessor)
		{
			in.readChunk(accessor.type);
			in.readChunk(accessor.componentType);
			in.readChunk(accessor.count);
			in.readChunk(accessor.bufferView);
			in.readChunk(accessor.byteOffset);
			return in;
		}
	};


} // avs
#pragma pack(pop)