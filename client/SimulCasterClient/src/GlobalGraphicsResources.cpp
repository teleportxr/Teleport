#include "GlobalGraphicsResources.h"

GlobalGraphicsResources GlobalGraphicsResources::instance;

GlobalGraphicsResources::GlobalGraphicsResources()
	:defaultPBREffect(&renderPlatform), defaultSkin(&renderPlatform, "Default Skin")
{

}
