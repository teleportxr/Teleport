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
