#include "GlobalGraphicsResources.h"

GlobalGraphicsResources *GlobalGraphicsResources::instance=nullptr;

GlobalGraphicsResources& GlobalGraphicsResources::GetInstance()
{
	if(!instance)
		instance=new GlobalGraphicsResources();
	return *instance;
}

GlobalGraphicsResources::GlobalGraphicsResources()
	:defaultPBREffect(&renderPlatform), defaultSkin(&renderPlatform, "Default Skin")
{
	mTagDataBuffer = renderPlatform.InstantiateShaderStorageBuffer();
}

void GlobalGraphicsResources::Init()
{
	scr::UniformBuffer::UniformBufferCreateInfo ub_ci;
	ub_ci.bindingLocation = 5;
	ub_ci.size = sizeof(PerMeshInstanceData);
	ub_ci.data =  &perMeshInstanceData;
	s_perMeshInstanceUniformBuffer = renderPlatform.InstantiateUniformBuffer();
	s_perMeshInstanceUniformBuffer->Create(&ub_ci);

	scr::ShaderResourceLayout perMeshInstanceBufferLayout;
	perMeshInstanceBufferLayout.AddBinding(5,
										   scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,
										   scr::Shader::Stage::SHADER_STAGE_VERTEX);
	perMeshInstanceShaderResource.SetLayout(perMeshInstanceBufferLayout);
	perMeshInstanceShaderResource.AddBuffer(scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,
			5,"u_PerMeshInstanceData"
			,{ s_perMeshInstanceUniformBuffer.get(), 0, sizeof(PerMeshInstanceData) });
}

const scr::ShaderResource& GlobalGraphicsResources::GetPerMeshInstanceShaderResource(const PerMeshInstanceData &p) const
{
	// I THINK this updates the values on the GPU...
	s_perMeshInstanceUniformBuffer->Update();
	return perMeshInstanceShaderResource;
}

std::string GlobalGraphicsResources::GenerateShaderPassName(bool lightmap,bool diffuse,bool normal,bool combined,bool emissive,int lightcount,bool highlight)
{
	std::string passname = "OpaquePBR";
	passname += "Lightmap";
	passname += lightmap ? "1" : "0";
	passname += "Diffuse";
	passname += diffuse ? "1" : "0";
	passname += "Normal";
	passname += normal ? "1" : "0";
	passname += "Combined";
	passname += combined ? "1" : "0";
	passname += "Emissive";
	passname += emissive ? "1" : "0";
	passname += "Lights";

	char countstr[10];
	sprintf(countstr,"%d",lightcount);
	passname += countstr;

	if(highlight)
	{
		passname += HIGHLIGHT_APPEND;
	}

	return passname;
}