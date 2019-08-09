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
			NEAREST,
			LINEAR,
			MIPMAP_NEAREST,
			MIPMAP_LINEAR
		};
		enum class Wrap :uint32_t
		{
			REPEAT,
			MIRRORED_REPEAT,
			CLAMP_TO_EDGE,
			CLAMP_TO_BORDER,
			MIRROR_CLAMP_TO_EDGE
		};
	protected:
		Filter m_MinFilter, m_MagFilter;
		Wrap m_WrapU, m_WrapV, m_WrapW;
		float m_MinLod, m_MaxLod;

		bool m_AnisotropyEnable;
		float m_MaxAnisotropy;

	public:
		virtual ~Sampler() {};

		virtual void Create(Filter filterMinMag[2], Wrap wrapUVW[3], float minLod, float maxLod, bool anisotropyEnable, float maxAnisotropy) = 0;
		virtual void Destroy() = 0;
		
		virtual void Bind() const  = 0;
		virtual void Unbind() const = 0;

		inline const Filter& GetMinFilter() const { return m_MinFilter; }
		inline const Filter& GetMagFilter() const { return m_MagFilter; }
		inline const Wrap& GetWrapU() const { return m_WrapU; }
		inline const Wrap& GetWrapV() const { return m_WrapV; }
		inline const Wrap& GetWrapW() const { return m_WrapW; }
		inline const float& GetMinLOD() const { return m_MinLod; }
		inline const float& GetMaxLOD() const { return m_MaxLod; }
		inline const bool& GetAnisotropyEnable() const {return m_AnisotropyEnable; }
		inline const float& GetMaxAnisotropy() const { return  m_MaxAnisotropy; }
	};
}