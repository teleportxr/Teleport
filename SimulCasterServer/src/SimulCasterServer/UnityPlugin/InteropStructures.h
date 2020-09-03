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
	avs::vec4 lightColour;

	size_t materialAmount;
	avs::uid* materialIDs;

	size_t childAmount;
	avs::uid* childIDs;

	operator avs::DataNode() const
	{
		return
		{
			transform,
			dataID,
			dataType,
			{materialIDs, materialIDs + materialAmount},
			lightColour,
			{childIDs, childIDs + childAmount}
		};
	}
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

	operator avs::Mesh() const
	{
		avs::Mesh newMesh;

		//Create vector in-place with pointer.
		newMesh.primitiveArrays = {primitiveArrays, primitiveArrays + primitiveArrayAmount};
		//Memcpy the attributes into a new memory location; the old location will be cleared/moved by C#'s garbage collector.
		for(int i = 0; i < primitiveArrayAmount; i++)
		{
			size_t dataSize = sizeof(avs::Attribute) * primitiveArrays[i].attributeCount;

			newMesh.primitiveArrays[i].attributes = new avs::Attribute[dataSize];
			memcpy_s(newMesh.primitiveArrays[i].attributes, dataSize, primitiveArrays[i].attributes, dataSize);
		}

		//Zip all of the maps back together.
		for(int i = 0; i < accessorAmount; i++)
		{
			newMesh.accessors[accessorIDs[i]] = accessors[i];
		}

		for(int i = 0; i < bufferViewAmount; i++)
		{
			newMesh.bufferViews[bufferViewIDs[i]] = bufferViews[i];
		}

		for(int i = 0; i < bufferAmount; i++)
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
	BSTR name;

	avs::PBRMetallicRoughness pbrMetallicRoughness;
	avs::TextureAccessor normalTexture;
	avs::TextureAccessor occlusionTexture;
	avs::TextureAccessor emissiveTexture;
	avs::vec3 emissiveFactor;

	size_t extensionAmount;
	avs::MaterialExtensionIdentifier* extensionIDs;
	avs::MaterialExtension** extensions;

	operator avs::Material() const
	{
		std::unordered_map<avs::MaterialExtensionIdentifier, std::shared_ptr<avs::MaterialExtension>> convertedExtensions;

		//Stitch extension map together.
		for(int i = 0; i < extensionAmount; i++)
		{
			avs::MaterialExtensionIdentifier extensionID = extensionIDs[i];

			switch(extensionID)
			{
				case avs::MaterialExtensionIdentifier::SIMPLE_GRASS_WIND:
					convertedExtensions.emplace(extensionID, std::make_shared<avs::SimpleGrassWindExtension>(*static_cast<avs::SimpleGrassWindExtension*>(extensions[i])));
					break;
			}
		}

		return
		{
			convertToByteString(name),
			pbrMetallicRoughness,
			normalTexture,
			occlusionTexture,
			emissiveTexture,
			emissiveFactor,
			convertedExtensions
		};
	}
};

struct InteropTexture
{
	BSTR name;

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

	operator avs::Texture() const
	{
		return
		{
			convertToByteString(name),
			width,
			height,
			depth,
			bytesPerPixel,
			arrayCount,
			mipCount,
			format,
			compression,
			dataSize,
			data,
			sampler_uid
		};
	}
};