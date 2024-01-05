// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "ClientRender/Resource.h"
#include "ClientRender/Texture.h"
#include "TeleportClient/basic_linear_algebra.h"
// For PbrMaterialConstants:
#include "client/Shaders/pbr_constants.sl"
namespace teleport
{
	namespace clientrender
	{
		class RenderPlatform;
		class Material : public Resource
		{
		public:
			struct MaterialParameter
			{
				avs::uid texture_uid = 0;
				std::shared_ptr<Texture> texture;		 // Texture Reference.
				vec2 texCoordsScale = {1, 1};			 // Scales the texture co-ordinates for lookup.
				vec4 textureOutputScalar = {1, 1, 1, 1}; // Scales the output of the texture per channel.
				int texCoordIndex = 0;					 // Selects which texture co-ordinates to use in sampling.
			};

			struct MaterialCreateInfo
			{
				std::string name;

				MaterialParameter diffuse;	// RGBA Colour Texture
				MaterialParameter normal;	// R: Tangent, G: Bi-normals and B: Normals
				MaterialParameter combined; // R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
				MaterialParameter emissive;
				int lightmapTexCoordIndex = 0;
				avs::uid uid = 0;	// session uid of the material.
				std::string shader; // not used if empty
				avs::MaterialMode materialMode = avs::MaterialMode::UNKNOWNMODE;
				bool doubleSided = false;
				bool clockwiseFaces = true;
			};

			struct MaterialData // Layout conformant to GLSL std140
			{
				vec4 diffuseOutputScalar;
				vec4 normalOutputScalar;
				vec4 combinedOutputScalarRoughMetalOcclusion;
				vec4 emissiveOutputScalar;

				vec2 diffuseTexCoordsScale;
				vec2 normalTexCoordsScale;
				vec2 combinedTexCoordsScale;
				vec2 emissiveTexCoordsScale;

				vec3 u_SpecularColour;
				int u_DiffuseTexCoordIndex;

				int u_NormalTexCoordIndex;
				int u_CombinedTexCoordIndex;
				int u_EmissiveTexCoordIndex;
				int u_LightmapTexCoordIndex;
			};

		protected:
			MaterialData m_MaterialData;
			MaterialCreateInfo m_CI;

		public:
			Material(const MaterialCreateInfo &pMaterialCreateInfo);
			~Material();

			std::string getName() const
			{
				return m_CI.name;
			}
			void SetMaterialCreateInfo(const MaterialCreateInfo &pMaterialCreateInfo);

			inline const MaterialCreateInfo &GetMaterialCreateInfo() const { return m_CI; }
			inline MaterialCreateInfo &GetMaterialCreateInfo() { return m_CI; }
			inline const MaterialData &GetMaterialData() const { return m_MaterialData; }

			void SetShaderOverride(const char *);
			platform::crossplatform::ConstantBuffer<PbrMaterialConstants, platform::crossplatform::ResourceUsageFrequency::ONCE> pbrMaterialConstants;
		};
	}
}