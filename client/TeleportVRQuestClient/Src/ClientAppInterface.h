#pragma once
#include "GlobalGraphicsResources.h"

class ClientAppInterface
{
public:
	virtual std::string LoadTextFile(const char *filename)=0;
	virtual const scr::Effect::EffectPassCreateInfo* BuildEffectPass(const char* effectPassName, scr::VertexBufferLayout* vbl, const scr::ShaderSystem::PipelineCreateInfo*, const std::vector<scr::ShaderResource>& shaderResources)=0;
	virtual void DrawTexture(avs::vec3 &offset,scr::Texture &texture) =0;
	virtual void PrintText(avs::vec3 &offset,avs::vec4 &colour,const char *txt,...)=0;
};