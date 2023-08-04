// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "ClientRender/Texture.h"
#include "TeleportClient/basic_linear_algebra.h"

namespace clientrender
{
	class RenderPlatform;
	class Material
	{
	public:
		struct MaterialParameter
		{
			avs::uid texture_uid=0;
			std::shared_ptr<Texture> texture;	//Texture Reference.
			vec2 texCoordsScalar[4] = { {1, 1}, {1, 1}, {1, 1}, {1, 1} };		//Scales the texture co-ordinates for tiling; one per channel.
			vec4 textureOutputScalar = { 1, 1, 1, 1 };		//Scales the output of the texture per channel.
			float texCoordIndex = 0.0f; //Selects which texture co-ordinates to use in sampling.
		};

		struct MaterialCreateInfo
		{
			std::string name;
			
			MaterialParameter diffuse;			//RGBA Colour Texture
			MaterialParameter normal;			//R: Tangent, G: Bi-normals and B: Normals
			MaterialParameter combined;			//R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
			MaterialParameter emissive;
			avs::uid uid=0;						// session uid of the material.
			std::string shader;					// not used if empty
			avs::MaterialMode materialMode=avs::MaterialMode::UNKNOWNMODE;
			bool doubleSided=false;
			bool clockwiseFaces=true;
		};

		struct MaterialData //Layout conformant to GLSL std140
		{
			vec4 diffuseOutputScalar;
			vec2 diffuseTexCoordsScalar_R;
			vec2 diffuseTexCoordsScalar_G;
			vec2 diffuseTexCoordsScalar_B;
			vec2 diffuseTexCoordsScalar_A;
			
			vec4 normalOutputScalar;
			vec2 normalTexCoordsScalar_R;
			vec2 normalTexCoordsScalar_G;
			vec2 normalTexCoordsScalar_B;
			vec2 normalTexCoordsScalar_A;
			
			vec4 combinedOutputScalarRoughMetalOcclusion;
			vec2 combinedTexCoordsScalar_R;
			vec2 combinedTexCoordsScalar_G;
			vec2 combinedTexCoordsScalar_B;
			vec2 combinedTexCoordsScalar_A;

			vec4 emissiveOutputScalar;
			vec2 emissiveTexCoordsScalar_R;
			vec2 emissiveTexCoordsScalar_G;
			vec2 emissiveTexCoordsScalar_B;
			vec2 emissiveTexCoordsScalar_A;
			
			vec3 u_SpecularColour;
			float _pad;

			float u_DiffuseTexCoordIndex;
			float u_NormalTexCoordIndex;
			float u_CombinedTexCoordIndex;
			float u_EmissiveTexCoordIndex;
		};

	protected:
		MaterialData m_MaterialData;
		MaterialCreateInfo m_CI;
	public:
		Material(const MaterialCreateInfo& pMaterialCreateInfo);
		void SetMaterialCreateInfo(const MaterialCreateInfo& pMaterialCreateInfo);

		inline const MaterialCreateInfo& GetMaterialCreateInfo() const { return m_CI; }
		inline MaterialCreateInfo& GetMaterialCreateInfo() { return m_CI; }
		inline const MaterialData& GetMaterialData() const { return m_MaterialData; }

		void SetShaderOverride(const char *);
		avs::uid id = 0;
	};
}