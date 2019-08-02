#pragma once

#include "Material.h"

using namespace scr;

Material::Material(Texture& diffuse, Texture& normal, Texture& combined)
	:m_Diffuse(diffuse), m_Normal(normal), m_Combined(combined) 
{
};

void Material::Bind()
{
	m_Diffuse.Bind();
	m_Normal.Bind();
	m_Combined.Bind();
}

void Material::Create()
{
	m_Diffuse.Create();
	m_Normal.Create();
	m_Combined.Create();
}
void Material::Destroy()
{
	m_Diffuse.Destroy();
	m_Normal.Destroy();
	m_Combined.Destroy();
}
