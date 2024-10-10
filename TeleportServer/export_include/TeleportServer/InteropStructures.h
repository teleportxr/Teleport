/*
 * The already defined structures in libavstream won't work for marshalling, due to usage of the standard library containers.
 * Instead of using an awkward interface everywhere, temporary structures are used to hold the data in a format that can be passed from managed C# code.
 */
#pragma once

#ifdef _MSC_VER
#include <wtypes.h>
#endif
#include <libavstream/common_exports.h>
#include <libavstream/common_maths.h>
#include <libavstream/common_networking.h>
#if TELEPORT_INTERNAL_INTEROP
#include <libavstream/geometry/mesh_interface.hpp>
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
#ifndef TELEPORT_PACKED
	#if defined(__GNUC__) || defined(__clang__)
		#define TELEPORT_PACKED __attribute__ ((packed,aligned(1)))
	#else
		#define TELEPORT_PACKED
	#endif
#endif
namespace teleport
{
	namespace core
	{
		struct Glyph;
		struct FloatKeyframe;
		struct Vector3Keyframe;
		struct Vector4Keyframe;
	}
}


struct CasterEncoderSettings
{
	int32_t frameWidth;
	int32_t frameHeight;
	int32_t depthWidth;
	int32_t depthHeight;
	bool wllWriteDepthTexture;
	bool enableStackDepth;
	bool enableDecomposeCube;
	float maxDepth;
	int32_t specularCubemapSize;
	int32_t roughCubemapSize;
	int32_t diffuseCubemapSize;
	int32_t lightCubemapSize;
} TELEPORT_PACKED;

//! Interop struct to receive nodes from external code.
struct InteropNode
{
	const char* name;					   // 8
										   //
	avs::Transform localTransform;		   // (3+4+3)*4==40 // 48
										   //
	uint8_t stationary;					   // 49
	avs::uid holder_client_id;			   // 57
										   //
	avs::NodeDataType dataType;			   // 58
	avs::uid parentID;					   // 66
	avs::uid dataID;					   // 74
	avs::uid skeletonID;				   // 82
										   //
	vec4 lightColour;					   // 98
	vec3 lightDirection;		// 110 constant, determined why whatever axis the engine uses for light direction.
	float lightRadius;			// 114 i.e. light is a sphere, where lightColour is the irradiance on its surface.
	float lightRange;			//118
	uint8_t lightType;			//119

	size_t jointCount;			//127
	int32_t* jointIndices;		//135

	size_t animationCount;		//143
	avs::uid* animationIDs;		//151

	size_t materialCount;		//159
	avs::uid* materialIDs;		//167
	
	avs::NodeRenderState renderState;	// 192

	int32_t priority;					// 196

	const char *url;					// 204
	const char *query_url;				// 212
#if TELEPORT_INTERNAL_INTEROP
	operator avs::Node() const
	{
		return
		{
			name,

			localTransform,

			stationary!=0,

			holder_client_id,

			priority,

			parentID,

			dataType,
			dataID,

			{materialIDs, materialIDs + materialCount},
			skeletonID,
			{jointIndices, jointIndices + jointCount},
			{animationIDs, animationIDs + animationCount},

			renderState,

			lightColour,
			lightRadius,
			lightDirection,
			lightType,
			lightRange,
			url ? url : "",
			query_url ? query_url : ""
		};


	}
#endif
};

struct InteropSkeleton
{
	char* name;
	char* path;

	size_t numBones;
	avs::uid* boneIDs;

	avs::Transform rootTransform;
#if TELEPORT_INTERNAL_INTEROP
	operator avs::Skeleton() const
	{
		return
		{
			name,
		//	{inverseBindMatrices, inverseBindMatrices + numInverseBindMatrices},
			{boneIDs, boneIDs + numBones},
			rootTransform,
			{},
			{},
			{},
		};
	}
#endif
};

struct InteropMesh
{
	const char * name;
	const char * path;

	int64_t primitiveArrayCount;
	avs::PrimitiveArray* primitiveArrays;

	int64_t accessorCount;
	avs::uid* accessorIDs;
	avs::Accessor* accessors;

	int64_t bufferViewCount;
	avs::uid* bufferViewIDs;
	avs::BufferView* bufferViews;

	int64_t bufferCount;
	avs::uid* bufferIDs;
	avs::GeometryBuffer* buffers;
	//The number of inverseBindMatrices MUST be greater than or equal to the number of joints referenced in the vertices.
	
	avs::uid inverseBindMatricesAccessorID;

#if TELEPORT_INTERNAL_INTEROP
	operator avs::Mesh() const
	{
		avs::Mesh newMesh;
		newMesh.name = name;

		//Create vector in-place with pointer.
		newMesh.primitiveArrays = {primitiveArrays, primitiveArrays + primitiveArrayCount};
		//Memcpy the attributes into a new memory location; the old location will be cleared/moved by C#'s garbage collector.
		for(int i = 0; i < primitiveArrayCount; i++)
		{
			size_t dataSize = sizeof(avs::Attribute) * primitiveArrays[i].attributeCount;

			newMesh.primitiveArrays[i].attributes = new avs::Attribute[dataSize];
			memcpy_s(newMesh.primitiveArrays[i].attributes, dataSize, primitiveArrays[i].attributes, dataSize);
		}

		//Zip all of the maps back together.
		for(int i = 0; i < accessorCount; i++)
		{
			newMesh.accessors[accessorIDs[i]] = accessors[i];
		}

		for(int i = 0; i < bufferViewCount; i++)
		{
			newMesh.bufferViews[bufferViewIDs[i]] = bufferViews[i];
		}

		for(int i = 0; i < bufferCount; i++)
		{
			newMesh.buffers[bufferIDs[i]] = buffers[i];

			//Memcpy the data into a new memory location; the old location will be cleared/moved by C#'s garbage collector.
			newMesh.buffers[bufferIDs[i]].data = new uint8_t[buffers[i].byteLength];
			memcpy_s(const_cast<uint8_t*>(newMesh.buffers[bufferIDs[i]].data), buffers[i].byteLength, buffers[i].data, buffers[i].byteLength);
		}
		newMesh.inverseBindMatricesAccessorID=inverseBindMatricesAccessorID;
		return newMesh;
	}
#endif
};

struct InteropMaterial
{
	const char* name;											// 8	//
	const char* path;											// 8	// 16
	avs::MaterialMode materialMode;								// 1	// 17
	avs::PBRMetallicRoughness pbrMetallicRoughness;				// 70	// 87
	avs::TextureAccessor normalTexture;							// 21	// 108
	avs::TextureAccessor occlusionTexture;						// 21	// 129
	avs::TextureAccessor emissiveTexture;						// 21	// 150
	vec3 emissiveFactor;										// 12	// 162
	bool doubleSided;											// 1	// 163
	uint8_t lightmapTexCoord;									// 1	// 164
	size_t extensionCount;										// 8	// 172
	avs::MaterialExtensionIdentifier* extensionIDs;				// 8	// 180
	avs::MaterialExtension** extensions;						// 8	// 188
	const InteropMaterial &operator=(const avs::Material& avsMaterial)
	{
		return *this;
	}
#if TELEPORT_INTERNAL_INTEROP
	operator avs::Material() const
	{
		std::unordered_map<avs::MaterialExtensionIdentifier, std::shared_ptr<avs::MaterialExtension>> convertedExtensions;

		//Stitch extension map together.
		for(int i = 0; i < (int)extensionCount; i++)
		{
			avs::MaterialExtensionIdentifier extensionID = extensionIDs[i];

			switch(extensionID)
			{
				case avs::MaterialExtensionIdentifier::SIMPLE_GRASS_WIND:
					convertedExtensions.emplace(extensionID, std::make_shared<avs::SimpleGrassWindExtension>(*static_cast<avs::SimpleGrassWindExtension*>(extensions[i])));
					break;
			}
		}

		avs::Material m=
		{
			avs::convertToByteString(name),
			materialMode,
			pbrMetallicRoughness,
			normalTexture,
			occlusionTexture,
			emissiveTexture,
			emissiveFactor,
			doubleSided,
			lightmapTexCoord,
			convertedExtensions
		};
		return m;
	}
#endif
};

struct InteropTexture
{
	const char* name=nullptr;					  // 8 = 8
	const char* path=nullptr;					  // 8 = 16
	 											  //  
	uint32_t width=0;							  // 4 = 20
	uint32_t height=0;							  // 4 = 24
	uint32_t depth=0;							  // 4 = 28
	uint32_t arrayCount=0;						  // 4 = 32
	uint32_t mipCount=0;						  // 4 = 36
												  //  
	avs::TextureFormat format;					  // 4 = 40
	avs::TextureCompression compression;		  // 4 = 44
	bool compressed=false;						  // 1 = 45
												  //   
	uint32_t dataSize=0;						  // 4 = 49
	unsigned char* data=nullptr;				  // 8 = 57
												  //  
	float valueScale=1.0f;						  // 4 = 61
												  //  
	bool cubemap=false;							  // 1 = 62
#if TELEPORT_INTERNAL_INTEROP
	operator avs::Texture() const
	{
		avs::Texture t=
		{
			name,
			width,
			height,
			depth,
			arrayCount,
			mipCount,
			cubemap,
			format,
			valueScale,
			compression,
			compressed
		};
		
		uint8_t *src=(uint8_t *)data;
		uint16_t numImages=*((uint16_t *)src);
		src += sizeof(uint16_t);
		std::vector<uint32_t> imageOffsets(numImages);
		memcpy( imageOffsets.data(),src, numImages * sizeof(uint32_t));
		imageOffsets.push_back(dataSize);
		src += numImages * sizeof(uint32_t);
		t.images.resize(numImages);
		for(size_t i=0;i<numImages;i++)
		{
			auto &image=t.images[i];
			size_t imgSize=imageOffsets[i+1]-imageOffsets[i];
			image.data.resize(imgSize);
			memcpy(image.data.data(),src,imgSize);
			src+=imgSize;
		}
		return t;
	}
#endif
};
static_assert (sizeof(InteropTexture) == 62, "InteropTexture Size is not correct");

struct InteropTransformKeyframe
{
	int16_t boneIndex=0;

	int numPositions=0;
	teleport::core::Vector3Keyframe* positionKeyframes=nullptr;

	int numRotations=0;
	teleport::core::Vector4Keyframe* rotationKeyframes=nullptr;

};

struct InteropTransformAnimation
{
	const char *name;	// 8
	const char *path;	// 8
	int64_t boneCount;	// 8
	InteropTransformKeyframe *boneKeyframes = nullptr; // 8
	float duration;									   // 4
};

struct InteropTextCanvas
{
	char* text=nullptr;
	char* font=nullptr;
	int size=0;
	float lineHeight=0.0f;
	float width=0;
	float height=0;
	vec4 colour;
};
namespace avs
{
	//! An input identifier, used between client and server to denote a specific input.
	typedef uint16_t InputId;
}

struct InteropInputState
{
	uint16_t numBinaryStates = 0;
	uint16_t numAnalogueStates= 0;
} TELEPORT_PACKED;
//! Input events that can only be in two states; e.g. button pressed or not.
struct InputEventBinary
{
	uint32_t eventID = 0;
	avs::InputId inputID = 0; //ID of the input type used that triggered the event.
	bool activated = false;
} AVS_PACKED;

struct InteropFontMap
{
	int size=0;
	int numGlyphs=0;
	teleport::core::Glyph *fontGlyphs=nullptr;
};

//! Struct to pass a font atlas back to the engine.
struct InteropFontAtlas
{
	const char * font_path=nullptr;
	int numMaps=0;
	InteropFontMap *fontMaps=nullptr;
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif
