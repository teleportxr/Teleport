// (C) Copyright 2018-2019 Simul Software Ltd

#include "PC_Sampler.h"

using namespace pc_client;
using namespace scr;

void PC_Sampler::Create(Filter filterMinMag[2], Wrap wrapUVW[3], float minLod, float maxLod, bool anisotropyEnable, float maxAnisotropy)
{
    m_MinFilter = filterMinMag[0];
    m_MagFilter = filterMinMag[1];
    m_WrapU = wrapUVW[0];
    m_WrapV = wrapUVW[1];
    m_WrapW = wrapUVW[2];
    m_MinLod = minLod;
    m_MaxLod = maxLod;

    m_AnisotropyEnable = anisotropyEnable;
    m_MaxAnisotropy = maxAnisotropy;
}

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