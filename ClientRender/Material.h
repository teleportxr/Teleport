// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "ClientRender/Resource.h"
#include "ClientRender/Texture.h"
#include "TeleportClient/basic_linear_algebra.h"
// For PbrMaterialConstants:
#include "client/assets/shaders/pbr_constants.sl"
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
				//! The texture's id in the cache this material belongs to, or zero where the material
				//! does not own the texture. A texture whose identity is a url - an image a .glb
				//! references as a separate file - is held by the session's cache and referred to
				//! here only by `texture`, so this stays zero for it. Do not test it to ask whether
				//! there is a texture; ask `hasTexture`.
				avs::uid texture_uid = 0;
				std::shared_ptr<Texture> texture;		 // Texture Reference.
				//! Whether the material declares a texture in this slot at all. Distinct from
				//! `texture`, which is a dummy (white/flat-normal/default-combined) when it does not,
				//! and distinct from `texture_uid`, which is zero for a texture owned by another
				//! cache. This is what selects the shader variant.
				bool hasTexture = false;
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
			static const char *getTypeName()
			{
				return "Material";
			}
			const std::string &getName() const
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