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

		//Update the UBO's data that is to be later submitted.
		void Update(size_t offset, size_t size, const void* data)
		{
			assert(m_Size >= offset + size);
			memcpy((void*)((uint64_t)m_Data + (uint64_t)offset), data, size);
		}
		//Submits the stored UBO data to the GPU.
		virtual void Submit() = 0;

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(UniformBuffer*, int)> ResourceInUseCallback = &UniformBuffer::ResourceInUse;
	};
}