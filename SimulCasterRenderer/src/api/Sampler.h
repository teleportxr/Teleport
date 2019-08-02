// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"

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
		Filter minFilter, magFilter;
		Wrap wrapU, wrapV, wrapW;
		float minLod, maxLod;

		bool anisotropyEnable;
		float maxAnisotropy;

	public:
		virtual ~Sampler() {};

		virtual void Create(Filter filterMinMag[2], Wrap wrapUVW[3], bool anisotropyEnable, float maxAnisotropy) = 0;
		virtual void Destroy() = 0;
		
		virtual void Bind() = 0;
		virtual void Unbind() = 0;
	};
}