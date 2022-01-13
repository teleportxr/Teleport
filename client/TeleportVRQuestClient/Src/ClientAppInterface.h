#pragma once
#include "GlobalGraphicsResources.h"

class ClientAppInterface
{
public:
	virtual std::string LoadTextFile(const char *filename)=0;
	virtual const clientrender::Effect::EffectPassCreateInfo* BuildEffectPass(const char* effectPassName, clientrender::VertexBufferLayout* vbl, const clientrender::ShaderSystem::PipelineCreateInfo*, const std::vector<clientrender::ShaderResource>& shaderResources)=0;
	virtual void DrawTexture(avs::vec3 &offset,clientrender::Texture &texture) =0;
	virtual void PrintText(avs::vec3 &offset,avs::vec4 &colour,const char *txt,...)=0;
};