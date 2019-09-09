
#include "ShaderSystem.h"
#include "../api/RenderPlatform.h"

using namespace scr;

ShaderSystem::Pipeline::Pipeline(scr::RenderPlatform *rp, const scr::ShaderSystem::PipelineCreateInfo *pc)
{
	m_Type           = pc->m_PipelineType;
	m_ShaderCount    = pc->m_Count;
	for (size_t i = 0; i < pc->m_Count; i++)
	{
		m_Shaders[i] = rp->InstantiateShader();
		m_Shaders[i]->Create(&pc->m_ShaderCreateInfo[i]);
	}
}
