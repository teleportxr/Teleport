// (C) Copyright 2018-2019 Simul Software Ltd

#include "PC_Sampler.h"

using namespace pc_client;
using namespace scr;

void PC_Sampler::Destroy()
{
    //NULL
}

void PC_Sampler::Bind() const
{
    //NULL
}
void PC_Sampler::Unbind() const
{
    //NULL
}

void pc_client::PC_Sampler::Create(SamplerCreateInfo * pSamplerCreateInfo)
{
	this->m_CI = *pSamplerCreateInfo;
}
