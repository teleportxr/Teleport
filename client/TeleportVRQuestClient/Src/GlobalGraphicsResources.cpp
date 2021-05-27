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

std::string GlobalGraphicsResources::GenerateShaderPassName(int diffuse,int normal,int combined,int emissive)
{
	std::string passname = "OpaquePBRDiffuse";
	passname += diffuse ? "1" : "0";
	passname += "Normal";
	passname += normal ? "1" : "0";
	passname += "Combined";
	passname += combined ? "1" : "0";
	passname += "Emissive";
	passname += emissive ? "1" : "0";
	return passname;
}