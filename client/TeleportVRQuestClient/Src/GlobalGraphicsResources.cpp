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