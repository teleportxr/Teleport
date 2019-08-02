// (C) Copyright 2018-2019 Simul Software Ltd

#include "Renderer.h"

using namespace scr;

Renderer::Renderer()
{
}

void Renderer::SubmitLight(const Light& light)
{
	m_Lights.push_back(&light);
}

void Renderer::SubmitObject(const Object& object)
{
	m_Objects.push_back(&object);
}

void Renderer::Execute()
{
}
