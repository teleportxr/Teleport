// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"

namespace scr
{
	//Interface for UniformBuffer
	class UniformBuffer
	{
	protected:
		uint32_t m_BindingLocation;
		size_t m_Size;
		const void* m_Data;

	public:
		UniformBuffer(size_t size, const void* data, uint32_t bindingLocation)
			:m_Size(size), m_Data(data), m_BindingLocation(bindingLocation) {};

		virtual ~UniformBuffer()
		{
			m_Size = 0;
			m_Data = nullptr;
		}

		virtual void Create() = 0;
		virtual void Destroy() = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Update(size_t offset, size_t size, const void* data) = 0;
	};
}