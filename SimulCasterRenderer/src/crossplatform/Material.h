// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/Texture.h"
#include "ShaderResource.h"
#include "api/Effect.h"
#include "api/RenderPlatform.h"
#include "basic_linear_algebra.h"

namespace scr
{
	class Material
	{
	public:
		struct MaterialParameter
		{
			std::shared_ptr<Texture> texture;	//Texture Reference.
			vec2 texCoordsScalar[4];			//Scales the texture co-ordinates for tiling; one per channel.
			vec4 textureOutputScalar;			//Scales the output of the texture per channel.
			float texCoordIndex;				//Selects which texture co-ordinates to use in sampling.
		};
		struct MaterialCreateInfo
		{
			const RenderPlatform* renderPlatform;
			MaterialParameter diffuse;	//RGBA Colour Texture
			MaterialParameter normal;	//R: Tangent, G: Bi-normals and B: Normals
			MaterialParameter combined;	//R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
			Effect* effect;				//Effect associated with this material: opaque, transparent, emissive, etc.
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

			vec4 combinedOutputScalar;
			vec2 combinedTexCoordsScalar_R;
			vec2 combinedTexCoordsScalar_G;
			vec2 combinedTexCoordsScalar_B;
			vec2 combinedTexCoordsScalar_A;

			vec3 u_SpecularColour;
			float _pad;

			float u_DiffuseTexCoordIndex;
			float u_NormalTexCoordIndex;
			float u_CombinedTexCoordIndex;
			float _pad2;
		};

	protected:
		MaterialData m_MaterialData;
		MaterialCreateInfo m_CI;


		static bool s_UninitialisedUB;
		static std::shared_ptr<UniformBuffer> s_UB;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;
	
	public:
		Material(const MaterialCreateInfo& pMaterialCreateInfo);

		inline const ShaderResource& GetShaderResource() const { return m_ShaderResource; }
		inline const MaterialCreateInfo& GetMaterialCreateInfoConst() const { return m_CI; }
		inline MaterialCreateInfo& GetMaterialCreateInfo() { return m_CI; }
		inline const MaterialData& GetMaterialData() { return m_MaterialData; }
	};
}