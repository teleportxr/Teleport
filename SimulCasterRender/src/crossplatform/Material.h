// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../api/Texture.h"

namespace scr
{
	class Material
	{
	protected:
		Texture& m_Diffuse;		//RGBA Colour Texture
		Texture& m_Normal;		//R: Tangent, G: Bi-normals and B: Normals
		Texture& m_Combined;	//R: Ambient Occlusion, G: Roughness, B: Metallic, A: Specular
	
	public:
		Material(Texture& diffuse, Texture& normal, Texture& combined);

		void Bind();

		void Create();
		void Destroy();
	};
}