/*
 * The already defined structures in libavstream won't work for marshalling, due to usage of the standard library containers.
 * Instead of using an awkward interface everywhere, temporary structures are used to hold the data in a format that can be passed from managed C# code.
 */
#pragma once

#include <libavstream/common.hpp>
#include <libavstream/platforms/this_platform.h>
#include "TeleportCore/Animation.h"
#include "TeleportCore/TextCanvas.h"
#include "libavstream/geometry/mesh_interface.hpp"
#ifdef _MSC_VER
#include <wtypes.h>
#endif


//! Interop struct to receive nodes from external code.
struct InteropNode
{
	const char* name;

	avs::Transform localTransform;

	uint8_t stationary;
	avs::uid holder_client_id;

	avs::NodeDataType dataType;
	avs::uid parentID;
	avs::uid dataID;
	avs::uid skeletonID;

	vec4 lightColour;
	vec3 lightDirection;		// constant, determined why whatever axis the engine uses for light direction.
	float lightRadius;				// i.e. light is a sphere, where lightColour is the irradiance on its surface.
	float lightRange;
	uint8_t lightType;

	size_t jointCount;
	int32_t* jointIndices;

	size_t animationCount;
	avs::uid* animationIDs;

	size_t materialCount;
	avs::uid* materialIDs;
	
	avs::NodeRenderState renderState;

	int32_t priority;

	const char *url;
	const char *query_url;

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
};

struct InteropSkeleton
{
	char* name;
	char* path;

	size_t numBones;
	avs::uid* boneIDs;

	avs::Transform rootTransform;

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
};

struct InteropMaterial
{
	const char* name;
	const char* path;
	avs::MaterialMode materialMode;
	avs::PBRMetallicRoughness pbrMetallicRoughness;
	avs::TextureAccessor normalTexture;
	avs::TextureAccessor occlusionTexture;
	avs::TextureAccessor emissiveTexture;
	vec3 emissiveFactor;
	bool doubleSided;
	uint8_t lightmapTexCoord;
	size_t extensionCount;
	avs::MaterialExtensionIdentifier* extensionIDs;
	avs::MaterialExtension** extensions;
	const InteropMaterial &operator=(const avs::Material& avsMaterial)
	{
		return *this;
	}
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
};

struct InteropTexture
{
	const char* name=nullptr;
	const char* path=nullptr;

	uint32_t width=0;
	uint32_t height=0;
	uint32_t depth=0;
	uint32_t bytesPerPixel=0;
	uint32_t arrayCount=0;
	uint32_t mipCount=0;

	avs::TextureFormat format;
	avs::TextureCompression compression;
	bool compressed=false;

	uint32_t dataSize=0;
	unsigned char* data=nullptr;

	avs::uid sampler_uid = 0;

	float valueScale=1.0f;

	bool cubemap=false;
	operator avs::Texture() const
	{
		return
		{
			name,
			width,
			height,
			depth,
			bytesPerPixel,
			arrayCount,
			mipCount,
			format,
			compression,
			compressed,
			sampler_uid,
			valueScale,
			cubemap,
			std::vector<uint8_t>(data, data+dataSize)
		};
	}
};

struct InteropTransformKeyframe
{
	int16_t boneIndex=0;

	int numPositions=0;
	teleport::core::Vector3Keyframe* positionKeyframes=nullptr;

	int numRotations=0;
	teleport::core::Vector4Keyframe* rotationKeyframes=nullptr;

	operator teleport::core::TransformKeyframeList() const
	{
		return
		{
			boneIndex,
			{positionKeyframes, positionKeyframes + numPositions},
			{rotationKeyframes, rotationKeyframes + numRotations}
		};
	}
};

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct InteropTransformAnimation
{
	const char *name;	// 8
	const char *path;	// 8
	int64_t boneCount;	// 8
	InteropTransformKeyframe *boneKeyframes = nullptr; // 8
	float duration;									   // 4

	operator teleport::core::Animation() const
	{
		return
		{
			name,
			duration,
			{boneKeyframes, boneKeyframes + boneCount}
		};
	}
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif

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

namespace teleport
{
	namespace core
	{
		struct Glyph;
	}
}

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
