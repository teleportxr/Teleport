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
		UniformBuffer(const scr::RenderPlatform*const r) :APIObject(r) {}
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

		//Submits the stored UBO data to the GPU.
		virtual void Submit() const = 0;
		// and... so does this? What is the difference?
		virtual void Update() const = 0;

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(UniformBuffer*, int)> ResourceInUseCallback = &UniformBuffer::ResourceInUse;

		inline UniformBufferCreateInfo& GetUniformBufferCreateInfo() {return m_CI;}

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
	};
}