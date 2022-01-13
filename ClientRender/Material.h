// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "ClientRender/Texture.h"
#include "ShaderResource.h"
#include "ClientRender/Effect.h"
#include "TeleportClient/basic_linear_algebra.h"

namespace clientrender
{
	class RenderPlatform;
	class Material
	{
	public:
		struct MaterialParameter
		{
			std::shared_ptr<Texture> texture;	//Texture Reference.
			avs::vec2 texCoordsScalar[4] = { {1, 1}, {1, 1}, {1, 1}, {1, 1} };		//Scales the texture co-ordinates for tiling; one per channel.
			avs::vec4 textureOutputScalar = { 1, 1, 1, 1 };		//Scales the output of the texture per channel.
			float texCoordIndex = 0.0f; //Selects which texture co-ordinates to use in sampling.
		};

		struct MaterialCreateInfo
		{
			std::string name;
			
			MaterialParameter diffuse;			//RGBA Colour Texture
			MaterialParameter normal;			//R: Tangent, G: Bi-normals and B: Normals
			MaterialParameter combined;			//R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
			MaterialParameter emissive;
			avs::uid uid;						//Effect associated with this material: opaque, transparent, emissive, etc.
		};

		struct MaterialData //Layout conformant to GLSL std140
		{
			avs::vec4 diffuseOutputScalar;
			avs::vec2 diffuseTexCoordsScalar_R;
			avs::vec2 diffuseTexCoordsScalar_G;
			avs::vec2 diffuseTexCoordsScalar_B;
			avs::vec2 diffuseTexCoordsScalar_A;
			
			avs::vec4 normalOutputScalar;
			avs::vec2 normalTexCoordsScalar_R;
			avs::vec2 normalTexCoordsScalar_G;
			avs::vec2 normalTexCoordsScalar_B;
			avs::vec2 normalTexCoordsScalar_A;
			
			avs::vec4 combinedOutputScalarRoughMetalOcclusion;
			avs::vec2 combinedTexCoordsScalar_R;
			avs::vec2 combinedTexCoordsScalar_G;
			avs::vec2 combinedTexCoordsScalar_B;
			avs::vec2 combinedTexCoordsScalar_A;

			avs::vec4 emissiveOutputScalar;
			avs::vec2 emissiveTexCoordsScalar_R;
			avs::vec2 emissiveTexCoordsScalar_G;
			avs::vec2 emissiveTexCoordsScalar_B;
			avs::vec2 emissiveTexCoordsScalar_A;
			
			avs::vec3 u_SpecularColour;
			float _pad;

			float u_DiffuseTexCoordIndex;
			float u_NormalTexCoordIndex;
			float u_CombinedTexCoordIndex;
			float u_EmissiveTexCoordIndex;
		};

	protected:
		MaterialData m_MaterialData;
		MaterialCreateInfo m_CI;

		std::shared_ptr<UniformBuffer> m_UB;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;
	
	public:
		Material(RenderPlatform* renderPlatform,const MaterialCreateInfo& pMaterialCreateInfo);
		void SetMaterialCreateInfo(RenderPlatform* renderPlatform,const MaterialCreateInfo& pMaterialCreateInfo);

		inline const ShaderResource& GetShaderResource() const { return m_ShaderResource; }
		inline const MaterialCreateInfo& GetMaterialCreateInfo() const { return m_CI; }
		inline MaterialCreateInfo& GetMaterialCreateInfo() { return m_CI; }
		inline const MaterialData& GetMaterialData() { return m_MaterialData; }
		avs::uid id = 0;
	};
}