// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

namespace scr
{
	//Interface for Sampler
	class Sampler
	{
	public:
		enum class Filter : uint32_t
		{
			UNKNOWN,
			NEAREST,
			LINEAR,
			MIPMAP_NEAREST,
			MIPMAP_LINEAR
		};
		enum class Wrap :uint32_t
		{
			UNKNOWN,
			REPEAT,
			MIRRORED_REPEAT,
			CLAMP_TO_EDGE,
			CLAMP_TO_BORDER,
			MIRROR_CLAMP_TO_EDGE
		};
		struct SamplerCreateInfo
		{
			Filter minFilter;
			Filter magFilter;
			Wrap wrapU;
			Wrap wrapV;
			Wrap wrapW;
			float minLod;
			float maxLod;
			bool anisotropyEnable;
			float maxAnisotropy;
		};
	
	protected:
		SamplerCreateInfo m_CI;

	public:
		virtual ~Sampler() 
		{
			m_CI.minFilter = Filter::UNKNOWN;
			m_CI.magFilter = Filter::UNKNOWN;
			m_CI.wrapU = Wrap::UNKNOWN;
			m_CI.wrapV = Wrap::UNKNOWN;
			m_CI.wrapW = Wrap::UNKNOWN;
			m_CI.minLod = 0.0f;
			m_CI.maxLod = 0.0f;
			m_CI.anisotropyEnable = false;
			m_CI.maxAnisotropy = 0.0f;
		};

		virtual void Create(SamplerCreateInfo* pSamplerCreateInfo) = 0;
		virtual void Destroy() = 0;
		
		virtual void Bind() const  = 0;
		virtual void Unbind() const = 0;

		inline const SamplerCreateInfo& GetSamplerCreateInfo() const { return m_CI; }
	};
}