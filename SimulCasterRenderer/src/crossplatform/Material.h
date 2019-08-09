// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/Texture.h"
#include "DescriptorSet.h"

namespace scr
{
	class Material
	{
	protected:
		Texture& m_Diffuse;		//RGBA Colour Texture
		Texture& m_Normal;		//R: Tangent, G: Bi-normals and B: Normals
		Texture& m_Combined;	//R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular

		DescriptorSetLayout m_SetLayout;
		DescriptorSet m_Set;
	
	public:
		Material(Texture& diffuse, Texture& normal, Texture& combined);

		inline const DescriptorSet& GetDescriptorSet() const { return m_Set; }
	};
}