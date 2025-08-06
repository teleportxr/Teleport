#include "MeshDecoder.h"
#include "GeometryDecoder.h"
#include "ResourceCreator.h"
#include "TeleportCore/DecodeMesh.h"
#include "libavstream/common_networking.h"
#include <regex>
#define TINYGLTF_IMPLEMENTATION
#undef TINYGLTF_NO_STB_IMAGE
#define tinygltf teleport_tinygltf
#include "tiny_gltf.h"
#undef teleport_tinygltf
#include "Eigen/Geometry"
#include <TeleportCore/Logging.h>
#include <ranges>
#pragma optimize("", off)
using namespace teleport;
using namespace clientrender;

int ToNumComponents(avs::Accessor::DataType d)
{
	switch (d)
	{
	case avs::Accessor::DataType::SCALAR:
		return 1;
	case avs::Accessor::DataType::VEC2:
		return 2;
	case avs::Accessor::DataType::VEC3:
		return 3;
	case avs::Accessor::DataType::VEC4:
		return 4;
	default:
		return 1;
		break;
	}
}

bool LoadImageDataFunction(teleport_tinygltf::Image *, const int, std::string *, std::string *, int, int, const unsigned char *, int, void *user_pointer)
{
	return true;
}

namespace teleport::core
{
	bool IsDataURI(const std::string &uri)
	{
		if (uri.size() < 1)
		{
			return false;
		}
		return true;
	}
	// Helper function to convert attribute semantic from glTF string to avs enum
	avs::AttributeSemantic ConvertAttributeSemantic(const std::string &semantic)
	{
		if (semantic == "POSITION")
		{
			return avs::AttributeSemantic::POSITION;
		}
		else if (semantic == "NORMAL")
		{
			return avs::AttributeSemantic::NORMAL;
		}
		else if (semantic == "TANGENT")
		{
			return avs::AttributeSemantic::TANGENT;
		}
		else if (semantic == "TEXCOORD_0")
		{
			return avs::AttributeSemantic::TEXCOORD_0;
		}
		else if (semantic == "TEXCOORD_1")
		{
			return avs::AttributeSemantic::TEXCOORD_1;
		}
		else if (semantic == "TEXCOORD_2")
		{
			return avs::AttributeSemantic::TEXCOORD_2;
		}
		else if (semantic == "TEXCOORD_3")
		{
			return avs::AttributeSemantic::TEXCOORD_3;
		}
		else if (semantic == "TEXCOORD_4")
		{
			return avs::AttributeSemantic::TEXCOORD_4;
		}
		else if (semantic == "TEXCOORD_5")
		{
			return avs::AttributeSemantic::TEXCOORD_5;
		}
		else if (semantic == "TEXCOORD_6")
		{
			return avs::AttributeSemantic::TEXCOORD_6;
		}
		else if (semantic == "COLOR_0")
		{
			return avs::AttributeSemantic::COLOR_0;
		}
		else if (semantic == "JOINTS_0")
		{
			return avs::AttributeSemantic::JOINTS_0;
		}
		else if (semantic == "WEIGHTS_0")
		{
			return avs::AttributeSemantic::WEIGHTS_0;
		}
		else if (semantic == "_UV2")
		{
			return avs::AttributeSemantic::TEXCOORD_2;
		}
		return avs::AttributeSemantic::COUNT; // Used as invalid/unknown
	}
	// Helper function to convert vec4 color
	vec4 ConvertColor(const std::vector<double> &color)
	{
		vec4 result = {0, 0, 0, 1}; // Default to black with alpha=1
		if (color.size() >= 3)
		{
			result.x = static_cast<float>(color[0]);
			result.y = static_cast<float>(color[1]);
			result.z = static_cast<float>(color[2]);
			if (color.size() >= 4)
			{
				result.w = static_cast<float>(color[3]);
			}
		}
		return result;
	}

	// Helper function to convert primitive mode from glTF to avs
	avs::PrimitiveMode ConvertPrimitiveMode(int mode)
	{
		switch (mode)
		{
		case TINYGLTF_MODE_POINTS:
			return avs::PrimitiveMode::POINTS;
		case TINYGLTF_MODE_LINE:
			return avs::PrimitiveMode::LINES;
		case TINYGLTF_MODE_TRIANGLES:
			return avs::PrimitiveMode::TRIANGLES;
		case TINYGLTF_MODE_LINE_STRIP:
			return avs::PrimitiveMode::LINE_STRIP;
		case TINYGLTF_MODE_TRIANGLE_STRIP:
			return avs::PrimitiveMode::TRIANGLE_STRIP;
		// Map unsupported modes to closest equivalents
		case TINYGLTF_MODE_LINE_LOOP:
			return avs::PrimitiveMode::LINE_STRIP;
		case TINYGLTF_MODE_TRIANGLE_FAN:
			return avs::PrimitiveMode::TRIANGLES;
		default:
			return avs::PrimitiveMode::TRIANGLES;
		}
	}

	// Helper function to convert component type from glTF to avs
	avs::ComponentType ConvertComponentType(int componentType)
	{
		switch (componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_BYTE:
			return avs::ComponentType::BYTE;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return avs::ComponentType::UBYTE;
		case TINYGLTF_COMPONENT_TYPE_SHORT:
			return avs::ComponentType::SHORT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return avs::ComponentType::USHORT;
		case TINYGLTF_COMPONENT_TYPE_INT:
			return avs::ComponentType::INT;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			return avs::ComponentType::UINT;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			return avs::ComponentType::FLOAT;
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:
			return avs::ComponentType::DOUBLE;
		default:
			return avs::ComponentType::FLOAT; // Default to FLOAT
		}
	}

	// Helper function to convert accessor type from glTF to avs
	avs::Accessor::DataType ConvertDataType(int type)
	{
		switch (type)
		{
		case TINYGLTF_TYPE_SCALAR:
			return avs::Accessor::DataType::SCALAR;
		case TINYGLTF_TYPE_VEC2:
			return avs::Accessor::DataType::VEC2;
		case TINYGLTF_TYPE_VEC3:
			return avs::Accessor::DataType::VEC3;
		case TINYGLTF_TYPE_VEC4:
			return avs::Accessor::DataType::VEC4;
		case TINYGLTF_TYPE_MAT4:
			return avs::Accessor::DataType::MAT4;
		default:
			return avs::Accessor::DataType::SCALAR;
		}
	}

	// Helper function to convert from glTF node TRS to Transform
	avs::Transform ConvertNodeTransform(const tinygltf::Node &node, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
	{
		avs::Transform transform;

		// Initialize with identity
		transform.position = {0.0f, 0.0f, 0.0f};
		transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f}; // Identity quaternion (w=1)
		transform.scale	   = {1.0f, 1.0f, 1.0f};

		// If we have a matrix, decompose it to TRS
		if (!node.matrix.empty())
		{
			// Convert the matrix to appropriate format
			mat4 matrix;
			for (int i = 0; i < 16; i++)
			{
				matrix.m[i] = static_cast<float>(node.matrix[i]);
			}

			// Decompose matrix to TRS
			// This would depend on your math library implementation
			// For example:
			// DecomposeMatrix(matrix, transform.position, transform.rotation, transform.scale);

			// Or use separate components if provided
		}
		else
		{
			// Use separate TRS components if available
			if (!node.translation.empty())
			{
				transform.position.x = static_cast<float>(node.translation[0]);
				transform.position.y = static_cast<float>(node.translation[1]);
				transform.position.z = static_cast<float>(node.translation[2]);
			}

			if (!node.rotation.empty())
			{
				// glTF uses XYZW quaternion format
				transform.rotation.x = static_cast<float>(node.rotation[0]);
				transform.rotation.y = static_cast<float>(node.rotation[1]);
				transform.rotation.z = static_cast<float>(node.rotation[2]);
				transform.rotation.w = static_cast<float>(node.rotation[3]);
			}

			if (!node.scale.empty())
			{
				transform.scale.x = static_cast<float>(node.scale[0]);
				transform.scale.y = static_cast<float>(node.scale[1]);
				transform.scale.z = static_cast<float>(node.scale[2]);
			}
		}

		// Convert transform from source axes standard to target
		avs::ConvertTransform(sourceStandard, targetStandard, transform);

		return transform;
	}

	static std::vector<std::string> name_order = {
		"hips",			"spine",		 "chest",		  "spine1",		  "upperchest",	   "spine2",	"neck",			 "head",
		"leftshoulder", "leftupperarm",	 "leftuparm",	  "leftlowerarm", "leftforearm",   "lefthand",	"rightshoulder", "rightupperarm",
		"rightuparm",	"rightlowerarm", "rightforearm",  "righthand",	  "leftupperleg",  "leftupleg", "leftlowerleg",	 "leftleg",
		"leftfoot",		"lefttoes",		 "rightupperleg", "rightupleg",	  "rightlowerleg", "rightleg",	"rightfoot",	 "righttoes"};
	bool RecurseNodeUids(const tinygltf::Node &root, uint64_t &next_id, const tinygltf::Model &model, std::vector<avs::uid> &node_uids, int max_recursion)
	{
		if (max_recursion <= 0)
		{
			return false;
		}
		// sort children in reproduceable order.
		auto sorted_children = root.children;
		/*std::sort(sorted_children.begin(),sorted_children.end(),[&model](int a,int b)
		{
			std::string nameA=GetMappedBoneName(model.nodes[a].name);
			std::string nameB=GetMappedBoneName(model.nodes[b].name);
			std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
			std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
			int A=(int)(std::ranges::find_if(name_order.begin(),name_order.end(),[&nameA](const std::string &str){return nameA.find(str) !=
		std::string::npos;})-name_order.begin()); int B=(int)(std::ranges::find_if(name_order.begin(),name_order.end(),[&nameB](const std::string &str){return
		nameB.find(str) != std::string::npos;})-name_order.begin()); return A<B;
		});*/
		// Set parent ID for each child
		for (int childIdx : sorted_children)
		{
			node_uids[childIdx]		 = next_id++;
			const tinygltf::Node &ch = model.nodes[childIdx];
			if (!RecurseNodeUids(ch, next_id, model, node_uids, max_recursion - 1))
			{
				return false;
			}
		}
		return true;
	}

	void GetBindMatrices(std::vector<mat4> &inverseBindMatrices, const tinygltf::Model &model, const tinygltf::Skin &skin)
	{
		const tinygltf::Accessor &accessor = model.accessors[skin.inverseBindMatrices];

		if (accessor.type == TINYGLTF_TYPE_MAT4 && accessor.count == skin.joints.size())
		{
			const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
			const tinygltf::Buffer	   &buffer	   = model.buffers[bufferView.buffer];

			// Calculate stride
			size_t stride						   = accessor.ByteStride(bufferView);
			if (stride == 0)
			{
				stride = sizeof(float) * 16; // Default for mat4
			}

			// Get data pointer
			const unsigned char *data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
			// Copy matrices
			inverseBindMatrices.resize(accessor.count);
			for (size_t j = 0; j < accessor.count; j++)
			{
				mat4		&matrix	   = inverseBindMatrices[j];
				const float *floatData = reinterpret_cast<const float *>(data + j * stride);
				for (int k = 0; k < 16; k++)
				{
					matrix.m[k] = floatData[k];
				}
				matrix.transpose();
			}
		}
	}

	bool ConvertGltfModelToDecodedGeometry(GeometryDecoder				 *dec,
										   const tinygltf::Model		 &model,
										   clientrender::ResourceCreator *target,
										   DecodedGeometry				 &dg,
										   const std::string			 &filename_url,
										   bool							  stationary)
	{
		json j				  = json::parse(model.extensions_json_string);
		// Do we have VRM extension? And is it the old v0.0 which points in the -Z direction?
		double vrmSpecVersion = 1.0;
		json   version;
		if (j.contains("VRM"))
		{
			json vrm = j["VRM"];
			if (vrm.contains("specVersion"))
			{
				version = vrm["specVersion"];
			}
		}
		else if (j.contains("VRMC_vrm"))
		{
			json vrm = j["VRMC_vrm"];
			if (vrm.contains("specVersion"))
			{
				version = vrm["specVersion"];
			}
		}

		if (version.is_number())
		{
			vrmSpecVersion = version;
		}
		else if (version.is_string())
		{
			vrmSpecVersion = std::atof(version.get<std::string>().c_str());
		}
		// Determine source axes standard - glTF uses right-handed Y-up system

		dg.next_id = 1; // Start with ID 1

		// Convert buffers
		for (size_t i = 0; i < model.buffers.size(); i++)
		{
			const tinygltf::Buffer &buffer = model.buffers[i];

			avs::GeometryBuffer		geometryBuffer;
			geometryBuffer.byteLength = buffer.data.size();

			// Create a copy of the buffer data
			geometryBuffer.data		  = new uint8_t[geometryBuffer.byteLength];
			std::memcpy(geometryBuffer.data, buffer.data.data(), geometryBuffer.byteLength);

			uint64_t bufferId	 = dg.next_id++;
			dg.buffers[bufferId] = std::move(geometryBuffer);
		}

		avs::uid first_bufferView = dg.next_id;
		// Convert buffer views
		for (size_t i = 0; i < model.bufferViews.size(); i++)
		{
			const tinygltf::BufferView &bufferView	 = model.bufferViews[i];

			uint64_t					bufferViewId = dg.next_id++;
			avs::BufferView				avsBufView;
			avsBufView.buffer = bufferView.buffer + 1; // Adjust index to match our new IDs
			// Which buffer does this point to?
			auto b			  = dg.buffers.find(avsBufView.buffer);
			if (b == dg.buffers.end())
			{
				TELEPORT_WARN("Bad mesh or scene data in {}", "");
				continue;
			}
			avs::GeometryBuffer &buffer = dg.buffers[avsBufView.buffer];
			avsBufView.byteOffset		= bufferView.byteOffset;
			avsBufView.byteLength		= bufferView.byteLength;
			if (avsBufView.byteOffset >= buffer.byteLength || avsBufView.byteLength > buffer.byteLength ||
				avsBufView.byteOffset + avsBufView.byteLength > buffer.byteLength)
			{
				TELEPORT_WARN("Bad mesh or scene data in {}", "");
				continue;
			}
			avsBufView.byteStride		 = bufferView.byteStride;

			dg.bufferViews[bufferViewId] = std::move(avsBufView);
		}
		avs::uid first_accessor = dg.next_id;
		// Convert accessors
		for (size_t i = 0; i < model.accessors.size(); i++)
		{
			uint64_t				  accessorId = dg.next_id++;
			const tinygltf::Accessor &accessor	 = model.accessors[i];

			avs::Accessor			  avsAccessor;
			avsAccessor.bufferView = accessor.bufferView + model.buffers.size() + 1; // Adjust index
			auto bv				   = dg.bufferViews.find(avsAccessor.bufferView);
			if (bv == dg.bufferViews.end())
			{
				TELEPORT_WARN("Bad mesh or scene data in {}", "");
				continue;
			}
			avsAccessor.byteOffset	  = accessor.byteOffset;
			avsAccessor.count		  = accessor.count;

			avsAccessor.componentType = ConvertComponentType(accessor.componentType);
			avsAccessor.type		  = ConvertDataType(accessor.type);

			size_t byteLength		  = ToNumComponents(avsAccessor.type) * avs::GetSizeOfComponentType(avsAccessor.componentType);
			if (avsAccessor.byteOffset >= bv->second.byteLength || byteLength > bv->second.byteLength ||
				avsAccessor.byteOffset + byteLength > bv->second.byteLength)
			{
				TELEPORT_WARN("Bad mesh or scene data in {}", "");
				continue;
			}

			dg.accessors[accessorId] = std::move(avsAccessor);
		}

		// Process materials
		avs::uid			  firstMaterial = dg.next_id;
		std::vector<avs::uid> material_uids(model.materials.size());
		for (size_t i = 0; i < model.materials.size(); i++)
		{
			const tinygltf::Material &material	   = model.materials[i];

			avs::uid				  material_uid = dg.next_id++;
			material_uids[i]					   = material_uid;

			avs::Material &avsMaterial			   = dg.internalMaterials[material_uid];
			avsMaterial.name					   = material.name;

			// Convert PBR material properties
			avsMaterial.materialMode			   = avs::MaterialMode::OPAQUE_MATERIAL;
			if (material.alphaMode == "BLEND")
			{
				avsMaterial.materialMode = avs::MaterialMode::TRANSPARENT_MATERIAL;
			}
			else if (material.alphaMode == "MASK")
			{
				avsMaterial.materialMode = avs::MaterialMode::MASKED_MATERIAL;
				// avsMaterial.alphaCutoff = material.alphaCutoff;
			}

			// PBR Metallic Roughness properties
			const auto &pbr											  = material.pbrMetallicRoughness;
			avsMaterial.pbrMetallicRoughness.baseColorFactor		  = ConvertColor(pbr.baseColorFactor);
			avsMaterial.pbrMetallicRoughness.metallicFactor			  = static_cast<float>(pbr.metallicFactor);
			avsMaterial.pbrMetallicRoughness.roughnessMultiplier	  = static_cast<float>(pbr.roughnessFactor);
			avsMaterial.pbrMetallicRoughness.roughnessOffset		  = 0.0f;

			// Initialize texture references
			avsMaterial.pbrMetallicRoughness.baseColorTexture		  = {0};
			avsMaterial.pbrMetallicRoughness.metallicRoughnessTexture = {0};
			avsMaterial.normalTexture								  = {0};
			avsMaterial.occlusionTexture							  = {0};
			avsMaterial.emissiveTexture								  = {0};

			// Base color texture
			if (pbr.baseColorTexture.index >= 0)
			{
				avsMaterial.pbrMetallicRoughness.baseColorTexture.index	   = pbr.baseColorTexture.index + dg.next_id; // Assuming we'll add textures later
				avsMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = pbr.baseColorTexture.texCoord;
			}

			// Metallic roughness texture
			if (pbr.metallicRoughnessTexture.index >= 0)
			{
				avsMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index	   = pbr.metallicRoughnessTexture.index + dg.next_id;
				avsMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = pbr.metallicRoughnessTexture.texCoord;
			}

			// Normal texture
			if (material.normalTexture.index >= 0)
			{
				avsMaterial.normalTexture.index	   = material.normalTexture.index + dg.next_id;
				avsMaterial.normalTexture.texCoord = material.normalTexture.texCoord;
				avsMaterial.normalTexture.scale	   = (float)material.normalTexture.scale;
			}

			// Occlusion texture
			if (material.occlusionTexture.index >= 0)
			{
				avsMaterial.occlusionTexture.index	  = material.occlusionTexture.index + dg.next_id;
				avsMaterial.occlusionTexture.texCoord = material.occlusionTexture.texCoord;
				avsMaterial.occlusionTexture.strength = (float)material.occlusionTexture.strength;
			}

			// Emissive texture
			if (material.emissiveTexture.index >= 0)
			{
				avsMaterial.emissiveTexture.index	 = material.emissiveTexture.index + dg.next_id;
				avsMaterial.emissiveTexture.texCoord = material.emissiveTexture.texCoord;
			}

			// Emissive factor
			avsMaterial.emissiveFactor = ConvertColor(material.emissiveFactor);
			//  avsMaterial.emissiveFactor.w = 1.0f; // Alpha is not used for emissive

			// Double sided
			avsMaterial.doubleSided	   = material.doubleSided;
		}

		// Add this to the ConvertGltfModelToDecodedGeometry function, right after processing materials
		// Process textures
		std::map<int, avs::uid>			texture_uids;
		std::map<avs::uid, std::string> texture_types;

		// First pass: assign UIDs to textures and prepare texture info
		for (int i = 0; i < (int)model.textures.size(); i++)
		{
			texture_uids[i] = dg.next_id++;
		}

		// Update material texture references with the correct UIDs
		for (size_t i = 0; i < model.materials.size(); i++)
		{
			const tinygltf::Material &material	   = model.materials[i];
			avs::uid				  material_uid = material_uids[i];
			auto					 &avsMaterial  = dg.internalMaterials[material_uid];
			avsMaterial.pbrMetallicRoughness.baseColorTexture.tiling.y *= -1.0f;
			// Base color texture
			if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
			{
				int		 texIndex										   = material.pbrMetallicRoughness.baseColorTexture.index;
				avs::uid texture_uid									   = texture_uids[texIndex];
				avsMaterial.pbrMetallicRoughness.baseColorTexture.index	   = texture_uid;
				avsMaterial.pbrMetallicRoughness.baseColorTexture.texCoord = material.pbrMetallicRoughness.baseColorTexture.texCoord;

				texture_types[texture_uid] += avsMaterial.name + "_diffuse";
			}

			// Metallic-roughness texture
			avsMaterial.pbrMetallicRoughness.metallicRoughnessTexture.tiling.y *= -1.0f;
			if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
			{
				int		 texIndex												   = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
				avs::uid texture_uid											   = texture_uids[texIndex];
				avsMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index	   = texture_uid;
				avsMaterial.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;
				texture_types[texture_uid] += avsMaterial.name + "_metallicRoughness";
			}

			// Normal texture
			if (material.normalTexture.index >= 0)
			{
				int		 texIndex				   = material.normalTexture.index;
				avs::uid texture_uid			   = texture_uids[texIndex];
				avsMaterial.normalTexture.index	   = texture_uid;
				avsMaterial.normalTexture.texCoord = material.normalTexture.texCoord;
				avsMaterial.normalTexture.scale	   = (float)material.normalTexture.scale;
				avsMaterial.normalTexture.tiling.y *= -1.0f;
				texture_types[texture_uid] += avsMaterial.name + "_normal";
			}

			// Occlusion texture
			if (material.occlusionTexture.index >= 0)
			{
				int		 texIndex					  = material.occlusionTexture.index;
				avs::uid texture_uid				  = texture_uids[texIndex];
				avsMaterial.occlusionTexture.index	  = texture_uid;
				avsMaterial.occlusionTexture.texCoord = material.occlusionTexture.texCoord;
				avsMaterial.occlusionTexture.strength = (float)material.occlusionTexture.strength;
				avsMaterial.occlusionTexture.tiling.y *= -1.0f;
				texture_types[texture_uid] += avsMaterial.name + "_occlusion";
			}

			// Emissive texture
			if (material.emissiveTexture.index >= 0)
			{
				int		 texIndex					 = material.emissiveTexture.index;
				avs::uid texture_uid				 = texture_uids[texIndex];
				avsMaterial.emissiveTexture.index	 = texture_uid;
				avsMaterial.emissiveTexture.texCoord = material.emissiveTexture.texCoord;
				avsMaterial.emissiveTexture.tiling.y *= -1.0f;
				texture_types[texture_uid] += avsMaterial.name + "_emissive";
			}
		}

		// If you have a ResourceCreator to handle textures, you can create them here
		if (target != nullptr)
		{
			for (int i = 0; i < (int)model.images.size(); i++)
			{
				const tinygltf::Image &image = model.images[i];

				// Find which texture(s) use this image
				for (int j = 0; j < (int)model.textures.size(); j++)
				{
					const tinygltf::Texture &texture = model.textures[j];

					if (texture.source == static_cast<int>(i))
					{
						avs::uid texture_uid				= texture_uids[j];

						// Determine image format and compression
						avs::TextureCompression compression = avs::TextureCompression::UNCOMPRESSED;
						if (image.mimeType == "image/jpeg")
						{
							compression = avs::TextureCompression::JPEG;
						}
						else if (image.mimeType == "image/png")
						{
							compression = avs::TextureCompression::PNG;
						}

						// For embedded or binary images
						if (!image.image.empty())
						{
							std::vector<uint8_t> imageData(image.image.begin(), image.image.end());

							// Create a name for the texture
							std::string name = !image.name.empty() ? image.name : (!image.uri.empty() ? image.uri : texture_types[texture_uid]);

							if (name.empty())
							{
								name		 = filename_url;
								size_t slash = filename_url.rfind("/");
								if (slash < filename_url.size())
								{
									name = filename_url.substr(slash + 1, filename_url.size() - slash - 1);
								}
								name += "_texture_" + std::to_string(j);
							}

							// Create texture
							if (compression != avs::TextureCompression::UNCOMPRESSED)
							{
								avs::Texture avsTexture = {
									name,									 // name
									static_cast<unsigned int>(image.width),	 // width
									static_cast<unsigned int>(image.height), // height
									0,										 // depth
									1,										 // array_size
									1,										 // mip_levels
									false,									 // is_cube
									avs::TextureFormat::RGBA8,				 // format
									1.0f,									 // gamma
									compression,							 // compression
									true									 // srgb
								};
								avsTexture.compressedData = std::move(imageData);
								target->CreateTexture(dg.server_or_cache_uid, texture_uid, avsTexture);
							}
						}
						// For external URI-based images
						else if (!image.uri.empty() && !IsDataURI(image.uri))
						{
							// In a full implementation, you would likely load the external image
							// and create a texture from it here
							// This would depend on your application's file loading capabilities
						}
						// For data URIs
						else if (!image.uri.empty() && IsDataURI(image.uri))
						{
							// Parse data URI and create texture
							std::string name = texture_types[texture_uid];
							if (name.empty())
							{
								name = filename_url + "_texture_" + std::to_string(j);
							}
							dec->decodeFromWeb(dg.server_or_cache_uid, image.uri, avs::GeometryPayloadType::Texture, target, texture_uid);
						}
						// For buffer view-based images
						else if (image.bufferView >= 0 && image.bufferView < static_cast<int>(model.bufferViews.size()))
						{
							const tinygltf::BufferView &bufferView = model.bufferViews[image.bufferView];
							const tinygltf::Buffer	   &buffer	   = model.buffers[bufferView.buffer];

							std::vector<uint8_t>		imageData(buffer.data.begin() + bufferView.byteOffset,
															  buffer.data.begin() + bufferView.byteOffset + bufferView.byteLength);

							std::string					name = !image.name.empty() ? image.name : texture_types[texture_uid];
							if (name.empty())
							{
								name = filename_url + "_texture_" + std::to_string(j);
							}

							avs::TextureCompression compression = avs::TextureCompression::UNCOMPRESSED;
							if (image.mimeType == "image/jpeg")
							{
								compression = avs::TextureCompression::JPEG;
							}
							else if (image.mimeType == "image/png")
							{
								compression = avs::TextureCompression::PNG;
							}

							if (compression != avs::TextureCompression::UNCOMPRESSED)
							{
								avs::Texture avsTexture = {
									name,									 // name
									static_cast<unsigned int>(image.width),	 // width
									static_cast<unsigned int>(image.height), // height
									0,										 // depth
									1,
									1,						   // array_size, mip_levels
									false,					   // is_cube
									avs::TextureFormat::RGBA8, // format
									1.0f,					   // gamma
									compression,			   // compression
									true					   // srgb
								};
								avsTexture.compressedData = std::move(imageData);
								target->CreateTexture(dg.server_or_cache_uid, texture_uid, avsTexture);
							}
						}
					}
				}
			}
		}

		// Process meshes and primitives
		std::vector<avs::uid> mesh_uids(model.meshes.size());
		for (size_t meshIndex = 0; meshIndex < model.meshes.size(); meshIndex++)
		{
			const tinygltf::Mesh &mesh	   = model.meshes[meshIndex];

			avs::uid			  mesh_uid = dg.next_id++;
			mesh_uids[meshIndex]		   = mesh_uid;

			std::vector<PrimitiveArray> primitives;

			for (size_t primIndex = 0; primIndex < mesh.primitives.size(); primIndex++)
			{
				const tinygltf::Primitive &primitive = mesh.primitives[primIndex];

				PrimitiveArray			   primArray;
				primArray.attributeCount = primitive.attributes.size();
				primArray.primitiveMode	 = ConvertPrimitiveMode(primitive.mode);

				// Set indices accessor if available
				if (primitive.indices >= 0)
				{
					primArray.indices_accessor = primitive.indices + first_accessor;
					if (dg.accessors.find(primArray.indices_accessor) == dg.accessors.end())
					{
						TELEPORT_WARN("Bad mesh or scene data in {}", "");
						continue;
					}
				}
				else
				{
					primArray.indices_accessor = 0; // No indices
				}

				// Set material if available
				if (primitive.material >= 0)
				{
					primArray.material = material_uids[primitive.material];
				}
				else
				{
					primArray.material = 0; // Default material
				}

				// Process attributes
				primArray.attributes.clear();
				for (const auto &attrib : primitive.attributes)
				{
					avs::Attribute attribute;
					attribute.semantic = ConvertAttributeSemantic(attrib.first);
					attribute.accessor = attrib.second + first_accessor;
					if (dg.accessors.find(attribute.accessor) == dg.accessors.end())
					{
						TELEPORT_WARN("Bad mesh or scene data in {}", "");
						continue;
					}
					primArray.attributes.push_back(attribute);
				}

				// Default transform (identity)
				primArray.transform = {1.0f, 1.0f, 1.0f, 0.0f}; // Scale factors and rotation angle

				primitives.push_back(std::move(primArray));
			}

			dg.primitiveArrays[mesh_uid] = std::move(primitives);
		}

		// Create a map to track node UIDs
		std::vector<avs::uid> node_uids(model.nodes.size());
		avs::uid			  first_node = dg.next_id;
		// First pass: Create all node uids.
		// consider skeletons, parent-child relationships.
		// We would ideally like to order the nodes in ascending
		// order within each hierarchy.

		std::set<int> root_indices;
		for (int i = 0; i < model.nodes.size(); i++)
		{
			root_indices.insert(i);
		}
		for (size_t i = 0; i < model.nodes.size(); i++)
		{
			const tinygltf::Node &gltfNode = model.nodes[i];
			for (size_t j = 0; j < gltfNode.children.size(); j++)
			{
				root_indices.erase(gltfNode.children[j]);
			}
		}
		for (int index : root_indices)
		{
			const tinygltf::Node &gltfNode = model.nodes[index];
			node_uids[index]			   = dg.next_id++;
			if (!RecurseNodeUids(gltfNode, dg.next_id, model, node_uids, 100))
			{
				TELEPORT_WARN("Failed max-recursion test in imported hierarchy for {}", filename_url);
				return false;
			}
		}
		//		Now fill in any left over.
		for (size_t i = 0; i < model.nodes.size(); i++)
		{
			if (!node_uids[i])
			{
				node_uids[i] = dg.next_id++;
			}
		}

		// Second pass: Set up nodes with proper properties and relationships
		for (size_t i = 0; i < model.nodes.size(); i++)
		{
			const tinygltf::Node &gltfNode = model.nodes[i];
			avs::uid			  nodeUid  = node_uids[i];

			avs::Node			  avsNode;
			avsNode.name					= gltfNode.name;
			avsNode.stationary				= stationary;
			avsNode.holder_client_id		= 0;
			avsNode.priority				= 0;

			// Initialize the transform
			avsNode.localTransform.position = {0.0f, 0.0f, 0.0f};
			avsNode.localTransform.rotation = {0.0f, 0.0f, 0.0f, 1.0f}; // Identity quaternion
			avsNode.localTransform.scale	= {1.0f, 1.0f, 1.0f};

			// Extract transformation data
			if (!gltfNode.matrix.empty())
			{
				// If matrix is provided, decompose it
				Eigen::Matrix4d matrix;
				for (int j = 0; j < 16; j++)
				{
					matrix.array()(j) = static_cast<float>(gltfNode.matrix[j]);
				}
				Eigen::Affine3d aff;
				aff								= matrix;

				auto tr							= aff.translation();
				avsNode.localTransform.position = vec3((float)tr.coeff(0), (float)tr.coeff(1), (float)tr.coeff(2));

				{
					auto			   rt = aff.rotation();
					Eigen::Quaterniond q(rt);
					avsNode.localTransform.rotation = {(float)q.x(), (float)q.y(), (float)q.z(), (float)q.w()};
				}
				// In a real implementation, you'd decompose this matrix
				// For now, we'll just use it as-is assuming your system handles matrix transforms
				// But typically you'd convert it to TRS components
				// DecomposeMatrix(matrix, avsNode.localTransform.position, avsNode.localTransform.rotation, avsNode.localTransform.scale);
			}
			else
			{
				// Use TRS components directly if available
				if (!gltfNode.translation.empty())
				{
					avsNode.localTransform.position.x = static_cast<float>(gltfNode.translation[0]);
					avsNode.localTransform.position.y = static_cast<float>(gltfNode.translation[1]);
					avsNode.localTransform.position.z = static_cast<float>(gltfNode.translation[2]);
				}

				if (!gltfNode.rotation.empty())
				{
					// glTF stores rotation as xyzw quaternion
					avsNode.localTransform.rotation.x = static_cast<float>(gltfNode.rotation[0]);
					avsNode.localTransform.rotation.y = static_cast<float>(gltfNode.rotation[1]);
					avsNode.localTransform.rotation.z = static_cast<float>(gltfNode.rotation[2]);
					avsNode.localTransform.rotation.w = static_cast<float>(gltfNode.rotation[3]);
				}

				if (!gltfNode.scale.empty())
				{
					avsNode.localTransform.scale.x = static_cast<float>(gltfNode.scale[0]);
					avsNode.localTransform.scale.y = static_cast<float>(gltfNode.scale[1]);
					avsNode.localTransform.scale.z = static_cast<float>(gltfNode.scale[2]);
				}
			}

			// Convert transform from source axes to target axes
			/*	if (sourceAxesStandard != targetAxesStandard)
			{
				avsNode.localTransform.position		   = platform::crossplatform::ConvertPosition((platform::crossplatform::AxesStandard)sourceAxesStandard,
																							  (platform::crossplatform::AxesStandard)targetAxesStandard,
																							  avsNode.localTransform.position);

				platform::crossplatform::Quaternionf q = platform::crossplatform::ConvertRotation((platform::crossplatform::AxesStandard)sourceAxesStandard,
																								  (platform::crossplatform::AxesStandard)targetAxesStandard,
																								  avsNode.localTransform.rotation);
				avsNode.localTransform.rotation		   = (const float *)&q;

				avsNode.localTransform.scale		   = platform::crossplatform::ConvertScale((platform::crossplatform::AxesStandard)sourceAxesStandard,
																					   (platform::crossplatform::AxesStandard)targetAxesStandard,
																					   avsNode.localTransform.scale);
			}*/

			// Set parent (will be filled in a separate pass)
			avsNode.parentID  = 0;

			// Initialize node data type to none
			avsNode.data_type = avs::NodeDataType::None;
			avsNode.data_uid  = 0;

			// Handle mesh references
			if (gltfNode.mesh >= 0 && gltfNode.mesh < static_cast<int>(mesh_uids.size()))
			{
				avsNode.data_type		   = avs::NodeDataType::Mesh;
				avsNode.data_uid		   = mesh_uids[gltfNode.mesh];

				// Add materials for the mesh
				const tinygltf::Mesh &mesh = model.meshes[gltfNode.mesh];
				for (const auto &primitive : mesh.primitives)
				{
					if (primitive.material >= 0 && primitive.material < static_cast<int>(material_uids.size()))
					{
						avsNode.materials.push_back(material_uids[primitive.material]);
					}
				}
			}

			// Initialize other fields with defaults
			avsNode.renderState	   = avs::NodeRenderState(); // Default render state
			avsNode.lightColour	   = {0.0f, 0.0f, 0.0f, 0.0f};
			avsNode.lightRadius	   = 0.0f;
			avsNode.lightDirection = {0.0f, 0.0f, 1.0f};
			avsNode.lightType	   = 0;
			avsNode.lightRange	   = 0.0f;

			// Store the node
			dg.nodes[nodeUid]	   = std::move(avsNode);
		}

		// Set up parent-child relationships
		for (size_t i = 0; i < model.nodes.size(); i++)
		{
			const tinygltf::Node &gltfNode = model.nodes[i];

			// Set parent ID for each child
			for (int childIdx : gltfNode.children)
			{
				// TODO: Prevent circular relationships
				if (childIdx >= 0 && childIdx < static_cast<int>(node_uids.size()))
				{
					auto &childNode	   = dg.nodes[node_uids[childIdx]];
					childNode.parentID = node_uids[i];
				}
			}
		}
		// Process skins and create skeletons
		std::map<int, avs::uid> skeleton_uids;
		avs::uid				first_skeleton = dg.next_id;
		// Possible to have multiple skeletons with different sets of nodes.
		// But equally, we want any skins that share the same set of joints to use the same skeleton.
		for (int i = 0; i < (int)model.skins.size(); i++)
		{
			const tinygltf::Skin &skin = model.skins[i];

			// Does this skin use an existing skeleton?
			// Either: same root, or
			//			This root is a child of an existing one, or
			//			An existing root is a child of this one.
			int		 root_index		   = skin.skeleton >= 0 ? skin.skeleton : skin.joints[0];
			avs::uid skeleton_uid	   = dg.next_id++;
			skeleton_uids[i]		   = skeleton_uid;

			avs::Skeleton &avsSkeleton = dg.skeletons[skeleton_uid];

			// Does any joint of this skeleton coincide with any joint of any other?
			// If so we must merge them.
			avs::uid rootBoneId		   = node_uids[root_index];
			avsSkeleton.rootBoneId	   = rootBoneId;
			if (skin.skeleton || skin.joints.size() > 0)
			{
				for (int jointIdx : skin.joints)
				{
					if (jointIdx >= 0 && jointIdx < static_cast<int>(node_uids.size()))
					{
						avs::uid id = node_uids[jointIdx];
						if (std::find(avsSkeleton.boneIDs.begin(), avsSkeleton.boneIDs.end(), id) == avsSkeleton.boneIDs.end())
						{
							avsSkeleton.boneIDs.push_back(id);
						}
					}
				}
			}
		}
		for (auto &sk1 : dg.skeletons)
		{
			avs::Skeleton &avsSkeleton1 = sk1.second;
			if (!avsSkeleton1.boneIDs.size())
			{
				continue;
			}
			auto &rootNode1 = dg.nodes[sk1.second.rootBoneId];
			// Should this skeleton be merged with another on the same hierarchy?
			for (auto &sk2 : dg.skeletons)
			{
				if (sk2.first == sk1.first)
				{
					continue;
				}
				avs::Skeleton &avsSkeleton2 = sk2.second;
				auto		  &rootNode2	= dg.nodes[sk2.second.rootBoneId];
				bool		   merge		= false;
				for (auto bone_uid : sk1.second.boneIDs)
				{
					// They share a common bone.
					if (std::find(sk2.second.boneIDs.begin(), sk2.second.boneIDs.end(), bone_uid) != sk2.second.boneIDs.end())
					{
						merge = true;
						break;
					}
				}
				if (!merge)
				{
					continue;
				}
				avs::uid id = avsSkeleton1.rootBoneId;
				while (dg.nodes[id].parentID != 0)
				{
					id = dg.nodes[id].parentID;
					if (id == avsSkeleton2.rootBoneId)
					{
						avsSkeleton1.rootBoneId = id;
						break;
					}
				}
				for (auto bone_uid : avsSkeleton2.boneIDs)
				{
					if (std::find(avsSkeleton1.boneIDs.begin(), avsSkeleton1.boneIDs.end(), bone_uid) != avsSkeleton1.boneIDs.end())
					{
						continue;
					}
					avsSkeleton1.boneIDs.push_back(bone_uid);
				}
				avsSkeleton2.boneIDs.clear();
				avsSkeleton2.rootBoneId = avsSkeleton1.rootBoneId;
				for (int i = 0; i < (int)skeleton_uids.size(); i++)
				{
					if (skeleton_uids[i] == sk2.first)
					{
						skeleton_uids[i] = sk1.first;
					}
				}
				break;
			}
		}
		std::set<avs::uid> skeleton_roots;
		// Set up skinned meshes and skeleton references
		for (int i = 0; i < (int)model.nodes.size(); i++)
		{
			const tinygltf::Node &gltfNode = model.nodes[i];
			avs::uid			  nodeUid  = node_uids[i];
			auto				 &avsNode  = dg.nodes[nodeUid];

			// Set up skin/skeleton
			if (gltfNode.skin >= 0 && gltfNode.skin < static_cast<int>(skeleton_uids.size()) && gltfNode.skin < model.skins.size())
			{
				const tinygltf::Skin &skin		   = model.skins[gltfNode.skin];
				avs::uid			  skeleton_uid = skeleton_uids[gltfNode.skin];
				if (!dg.skeletons[skeleton_uid].boneIDs.size())
				{
					for (int j = 0; j < skeleton_uids.size(); j++)
					{
						avs::Skeleton &sk = dg.skeletons[skeleton_uids[i]];
						if (sk.boneIDs.size() > 0 && sk.rootBoneId == dg.skeletons[skeleton_uid].rootBoneId)
						{
							skeleton_uid = skeleton_uids[i];
							break;
						}
					}
				}
				avs::Skeleton &avsSkeleton = dg.skeletons[skeleton_uid];
				int			   root_index  = skin.skeleton >= 0 ? skin.skeleton : skin.joints[0];
				avsNode.skeletonNodeID	   = avsSkeleton.rootBoneId;
				skeleton_roots.insert(avsNode.skeletonNodeID);
				// Set root node
				if (!avsSkeleton.boneIDs.empty())
				{
					GetBindMatrices(avsNode.inverseBindMatrices, model, skin);
					// Set up joint indices mapping
					//   For each inv bind matrix, what is its index in the skeleton?
					avsNode.joint_indices.resize(skin.joints.size());

					for (size_t j = 0; j < skin.joints.size(); j++)
					{
						avs::uid joint_uid = node_uids[skin.joints[j]];
						uint16_t j_index =
							(uint16_t)(std::find(avsSkeleton.boneIDs.begin(), avsSkeleton.boneIDs.end(), joint_uid) - avsSkeleton.boneIDs.begin());
						avsNode.joint_indices[j] = j_index;
					}
				}
			}
		}
		// Place skeletons in their appropriate root nodes:
		for (auto &s : skeleton_uids)
		{
			avs::Skeleton &avsSkeleton = dg.skeletons[s.second];
			if (avsSkeleton.boneIDs.size() == 0)
			{
				continue;
			}
			auto &rootNode		= dg.nodes[avsSkeleton.rootBoneId];
			rootNode.skeletonID = s.second;
		}
		if (vrmSpecVersion < 1.0)
		{
			//	dg.vrmFixRotation=true;
			for (auto r : skeleton_roots)
			{
				auto &rootNode = dg.nodes[r];
				{
					platform::crossplatform::Quaternionf q = rootNode.localTransform.rotation;
					q.Rotate(PI, {0, 1.0f, 0});
					rootNode.localTransform.rotation = {q.x, q.y, q.z, q.s};
				}
			}
		}


		// Set clockwise faces flag
		// glTF uses counter-clockwise winding by default
		dg.clockwiseFaces = false;
		return true;
	}

}

// namespace teleport::core
bool GeometryDecoder::DecodeScene(const teleport::clientrender::GeometryDecodeData &geometryDecodeData, core::DecodedGeometry &dg, bool stationary)
{
	teleport_tinygltf::Model	model;
	teleport_tinygltf::TinyGLTF loader;
	std::string					err;
	std::string					warn;

	// teleport_tinygltf::SetImageLoader( &LoadImageDataFunction, nullptr) ;

	const uint8_t *source = geometryDecodeData.data.data() + geometryDecodeData.offset;
	uint32_t	   sz	  = (uint32_t)geometryDecodeData.bytesRemaining();
	try
	{
		// Get extensions, we might want to look at VRM properties.
		loader.SetStoreOriginalJSONForExtrasAndExtensions(true);
		bool ret = loader.LoadBinaryFromMemory(&model, &err, &warn, source, sz); // for binary glTF(.glb)

		if (!warn.empty())
		{
			TELEPORT_WARN("Warn: {}", warn.c_str());
		}

		if (!err.empty())
		{
			TELEPORT_WARN("Err: {}", err.c_str());
		}

		if (!ret)
		{
			TELEPORT_WARN("Failed to parse glTF\n");
			return false;
		}
	}
	catch (...)
	{
		return false;
	}
	return ConvertGltfModelToDecodedGeometry(this, model, geometryDecodeData.target, dg, geometryDecodeData.filename_or_url, stationary);
}
