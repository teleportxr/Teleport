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
		virtual ~UniformBuffer()
		{
			m_Size = 0;
			m_Data = nullptr;
		}

		//Binding Locations for UBOs
		//Camera = 0;
		//Model = 1;
		//Light = 2;
		virtual void Create(size_t size, const void* data, uint32_t bindingLocation) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Update(size_t offset, size_t size, const void* data) = 0;
	};
}