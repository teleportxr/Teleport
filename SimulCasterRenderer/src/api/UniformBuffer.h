// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

namespace scr
{
	//Interface for UniformBuffer
	class UniformBuffer : public APIObject
	{
	public:
		struct UniformBufferCreateInfo
		{
			uint32_t bindingLocation;
			size_t size;
			const void* data;
		};

	protected:
		UniformBufferCreateInfo m_CI;

	public:
		UniformBuffer(scr::RenderPlatform *r) :APIObject(r) {}
		virtual ~UniformBuffer()
		{
			m_CI.bindingLocation = 0;
			m_CI.size = 0;
			m_CI.data = nullptr;
		}

		//Binding Locations for UBs from 0 - 9
		//Camera = 0;
		//ModelTransform = 1;
		//Light = 2;
		//Material = 3;
		virtual void Create(UniformBufferCreateInfo* pUniformBuffer) = 0;
		virtual void Destroy() = 0;

		//Update the UBO's data that is to be later submitted.
		void Update(size_t offset, size_t size, const void* data)
		{
			assert(m_CI.size >= offset + size);
			memcpy((void*)((uint64_t)m_CI.data + (uint64_t)offset), data, size);
		}
		//Submits the stored UBO data to the GPU.
		virtual void Submit() const = 0;

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(UniformBuffer*, int)> ResourceInUseCallback = &UniformBuffer::ResourceInUse;

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}