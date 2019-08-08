// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"

//Graphics API
namespace scr
{
	class API
	{
	public:
		enum class APIType : uint32_t
		{
			UNKNOWN,
			D3D11,
			D3D12,
			OPENGL,
			OPENGLES,
			VULKAN
		};

	private:
		static APIType s_API;

	public:
		static inline void SetAPI(APIType api) { s_API = api; }
		static inline const APIType& GetAPI() { return s_API; }
	};
}