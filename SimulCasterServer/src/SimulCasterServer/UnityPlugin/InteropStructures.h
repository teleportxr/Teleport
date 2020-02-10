/*
 * The already defined structures in libavstream won't work for marshalling, due to usage of the standard library containers.
 * Instead of using an awkward interface everywhere, temporary structures are used to hold the data in a format that can be passed from managed C# code.
 */
#pragma once

#include "libavstream/common.hpp"
#include "libavstream/geometry/mesh_interface.hpp"

struct InteropNode
{
	avs::Transform transform;
	avs::uid dataID;
	avs::NodeDataType dataType;

	size_t materialAmount;
	avs::uid* materialIDs;

	size_t childAmount;
	avs::uid* childIDs;
};

struct InteropMesh
{
	int64_t primitiveArrayAmount;
	avs::PrimitiveArray* primitiveArrays;

	int64_t accessorAmount;
	avs::uid* accessorIDs;
	avs::Accessor* accessors;

	int64_t bufferViewAmount;
	avs::uid* bufferViewIDs;
	avs::BufferView* bufferViews;

	int64_t bufferAmount;
	avs::uid* bufferIDs;
	avs::GeometryBuffer* buffers;
};

struct InteropMaterial
{
	size_t nameLength;
	char* name;

	avs::PBRMetallicRoughness pbrMetallicRoughness;
	avs::TextureAccessor normalTexture;
	avs::TextureAccessor occlusionTexture;
	avs::TextureAccessor emissiveTexture;
	avs::vec3 emissiveFactor;

	size_t extensionAmount;
	avs::MaterialExtensionIdentifier* extensionIDs;
	avs::MaterialExtension** extensions;
};

struct InteropTexture
{
	size_t nameLength;
	char* name;

	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t bytesPerPixel;
	uint32_t arrayCount;
	uint32_t mipCount;

	avs::TextureFormat format;
	avs::TextureCompression compression;

	uint32_t dataSize;
	unsigned char* data;

	avs::uid sampler_uid = 0;
};

//Converts an interop texture to a libavstream texture.
//	texture : Texture to copy data from.
//	copyBuffer : Whether to copy the texture buffer from the InteropTexture instead of copying the pointer. This should be true, unless you have copied the buffer yourself.
//Returns avs::Texture with the same data as the InteropTexture, ready to be stored.
avs::Texture ToAvsTexture(InteropTexture texture, bool copyBuffer)
{
	unsigned char* data;
		
	if(copyBuffer)
	{
		data = new unsigned char[texture.dataSize];
		memcpy_s(data, texture.dataSize, texture.data, texture.dataSize);
	}
	else
	{
		data = texture.data;
	}

	return
	{
		{texture.name, texture.name + texture.nameLength},
		texture.width,
		texture.height,
		texture.depth,
		texture.bytesPerPixel,
		texture.arrayCount,
		texture.mipCount,
		texture.format,
		texture.compression,
		texture.dataSize,
		data,
		texture.sampler_uid
	};
}