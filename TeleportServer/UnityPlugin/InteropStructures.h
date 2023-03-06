/*
 * The already defined structures in libavstream won't work for marshalling, due to usage of the standard library containers.
 * Instead of using an awkward interface everywhere, temporary structures are used to hold the data in a format that can be passed from managed C# code.
 */
#pragma once

#include <libavstream/common.hpp>
#include <libavstream/platforms/this_platform.h>
#include "TeleportCore/AnimationInterface.h"
#include "TeleportCore/TextCanvas.h"
#include "libavstream/geometry/mesh_interface.hpp"
#ifdef _MSC_VER
#include <wtypes.h>
#endif


//! Interop struct to receive nodes from external code.
struct InteropNode
{
	char* name;

	avs::Transform localTransform;
	avs::Transform globalTransform;

	uint8_t stationary;
	avs::uid holder_client_id;

	avs::NodeDataType dataType;
	avs::uid parentID;
	avs::uid dataID;
	avs::uid skinID;

	avs::vec4 lightColour;
	avs::vec3 lightDirection;		// constant, determined why whatever axis the engine uses for light direction.
	float lightRadius;				// i.e. light is a sphere, where lightColour is the irradiance on its surface.
	float lightRange;
	uint8_t lightType;

	size_t animationCount;
	avs::uid* animationIDs;

	size_t materialCount;
	avs::uid* materialIDs;
	
	avs::NodeRenderState renderState;

	size_t childCount;
	avs::uid* childIDs;

	int32_t priority;

	operator avs::Node() const
	{
		return
		{
			name,

			localTransform,
			globalTransform,

			stationary!=0,

			holder_client_id,

			priority,

			parentID,
			{childIDs, childIDs + childCount},

			dataType,
			dataID,

			{materialIDs, materialIDs + materialCount},
			skinID,
			{animationIDs, animationIDs + animationCount},

			renderState,

			lightColour,
			lightRadius,
			lightDirection,
			lightType,
			lightRange
		};
	}
};

struct InteropSkin
{
	char* name;
	char* path;

	size_t numInverseBindMatrices;
	avs::Mat4x4* inverseBindMatrices;

	size_t numBones;
	avs::uid* boneIDs;
	
	size_t numJoints;
	avs::uid* jointIDs;

	avs::Transform rootTransform;

	operator avs::Skin() const
	{
		return
		{
			name,
			{inverseBindMatrices, inverseBindMatrices + numInverseBindMatrices},
			{boneIDs, boneIDs + numBones},
			{jointIDs, jointIDs + numJoints},
			rootTransform,
			{},
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
	avs::vec3 emissiveFactor;

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
			dataSize,
			data
		};
	}
};

struct InteropTransformKeyframe
{
	size_t boneIndex=0;

	int numPositions=0;
	avs::Vector3Keyframe* positionKeyframes=nullptr;

	int numRotations=0;
	avs::Vector4Keyframe* rotationKeyframes=nullptr;

	operator avs::TransformKeyframeList() const
	{
		return
		{
			boneIndex,
			{positionKeyframes, positionKeyframes + numPositions},
			{rotationKeyframes, rotationKeyframes + numRotations}
		};
	}
};

struct InteropTransformAnimation
{
	const char *name;
	const char * path;
	int64_t boneCount;
	InteropTransformKeyframe* boneKeyframes=nullptr;

	operator avs::Animation() const
	{
		return
		{
			name,
			{boneKeyframes, boneKeyframes + boneCount}
		};
	}
};

struct InteropTextCanvas
{
	char* text=nullptr;
	char* font=nullptr;
	int size=0;
	float lineHeight=0.0f;
	float width=0;
	float height=0;
	avs::vec4 colour;
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
