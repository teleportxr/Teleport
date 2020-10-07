#include "GlobalGraphicsResources.h"

GlobalGraphicsResources GlobalGraphicsResources::instance;


scc::GL_Effect *GlobalGraphicsResources::GetPbrEffect()
{
	return &pbrEffect;
}