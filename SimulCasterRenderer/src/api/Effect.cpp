
#include "Effect.h"
using namespace scr;
const Effect::EffectPassCreateInfo *Effect::GetEffectPassCreateInfo(const char* effectPassName) const
{
	const auto &e=m_EffectPasses.find(effectPassName);
	if(e==m_EffectPasses.end())
		return nullptr;
	return &e->second;
}