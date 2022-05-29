// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"

namespace clientrender
{
	//Interface for UniformBuffer
	class UniformBuffer : public APIObject
	{
	public:
		struct UniformBufferCreateInfo
		{
			std::string name;
			uint32_t bindingLocation;
			size_t size;
			const void* data;
		};

	protected:
		UniformBufferCreateInfo m_CI;

	public:
		UniformBuffer(const clientrender::RenderPlatform* const r)
			:APIObject(r), m_CI()
		{}

		virtual ~UniformBuffer()
		{
			m_CI.bindingLocation=0;
			m_CI.size=0;
			m_CI.data = nullptr;
		}

		//Binding Locations for UBs from 0 - 9
		//Camera{}
		//ModelTransform = 1;
		//Light = 2;
		//Material = 3;
		virtual void Create(UniformBufferCreateInfo* pUniformBuffer) 
		{
			m_CI = *pUniformBuffer;
		}
		virtual void Destroy(){}

		//Submits the stored UBO data to the GPU.
		virtual void Submit() const{}
		// and... so does this? What is the difference?
		virtual void Update() const{}

		virtual bool ResourceInUse(int timeout){return true;}
		std::function<bool(UniformBuffer*, int)> ResourceInUseCallback = &UniformBuffer::ResourceInUse;

		inline UniformBufferCreateInfo& GetUniformBufferCreateInfo() {return m_CI;}

	protected:
		virtual void Bind() const{}
		virtual void Unbind() const{}
	};
}