// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"

namespace scr
{
	class VertexBufferLayout
	{
	public:
		enum class Type : uint32_t
		{
			FLOAT,
			UINT,
			INT
		};
		enum class ComponentCount : uint32_t
		{
			SINGLE = 1,
			VEC2,
			VEC3,
			VEC4
		};
		struct VertexAttribute
		{
			uint32_t location;
			ComponentCount compenentCount;
			Type type;
		};

	public:
		std::vector<VertexAttribute> m_Attributes;

		inline void AddAttribute(const VertexAttribute& attribute)
		{
			m_Attributes.push_back(attribute);
		}

		inline void AddAttribute(uint32_t location, ComponentCount count, Type type)
		{
			m_Attributes.push_back({ location, count, type });
		}
	};
}