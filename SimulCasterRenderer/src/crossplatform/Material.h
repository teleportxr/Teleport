// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/Texture.h"
#include "DescriptorSet.h"
#include "api/Effect.h"
#include "basic_linear_algebra.h"

namespace scr
{
	class Material :public APIObject
	{
	public:
		struct MaterialParameter
		{
			std::shared_ptr<Texture> texture;	//Texture Reference.
			vec2 texCoordsScalar[4];			//Scales the texture co-ordinates for tiling; one per channel .
			vec4 textureOutputScalar;			//Scales the output of the texture per channel.
		};
		struct MaterialCreateInfo
		{
			MaterialParameter diffuse;	//RGBA Colour Texture
			MaterialParameter normal;	//R: Tangent, G: Bi-normals and B: Normals
			MaterialParameter combined;	//R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
			Effect* effect;				//Effect associated with this material: opaque, transparent, emissive, etc.
		};

	protected:
		MaterialCreateInfo m_CI;

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
		} m_MaterialData;

		static bool s_UninitialisedUB;
		std::unique_ptr<UniformBuffer> m_UB;

		DescriptorSetLayout m_SetLayout;
		DescriptorSet m_Set;
	
	public:
		Material(RenderPlatform* r,MaterialCreateInfo* pMaterialCreateInfo);

		void UpdateMaterialUB();

		inline const DescriptorSet& GetDescriptorSet() const { return m_Set; }
		inline const MaterialCreateInfo& GetMaterialCreateInfoConst() const { return m_CI; }
		inline MaterialCreateInfo& GetMaterialCreateInfo() { return m_CI; }
	};
}