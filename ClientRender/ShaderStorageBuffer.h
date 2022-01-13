// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

namespace scr
{
	//Interface for ShaderStorageBuffer
	class ShaderStorageBuffer : public APIObject
	{
	public:
		enum class Access : uint32_t
		{
			NONE = 0,
			READ_BIT = 0x00000001,
			WRITE_BIT = 0x00000002,
			READ_WRITE_BIT = 0x00000003
		};
		struct ShaderStorageBufferCreateInfo
		{
			uint32_t bindingLocation;
			Access access;
			size_t size;
			void* data;
		};

	protected:
		ShaderStorageBufferCreateInfo m_CI;

	public:
		ShaderStorageBuffer(const scr::RenderPlatform* const r)
			: APIObject(r), m_CI()
		{}

		virtual ~ShaderStorageBuffer()
		{
			m_CI.bindingLocation = 0;
			m_CI.access = Access::READ_BIT;
			m_CI.size = 0;
			m_CI.data = nullptr;
		}

		virtual void Create(ShaderStorageBufferCreateInfo* pShaderStorageBufferCreateInfo) = 0;
		virtual void Update(size_t size, const void* data, uint32_t offset = 0) = 0;
		virtual void* Map() = 0;
		virtual void Unmap() = 0;
		virtual void Destroy() = 0;

		inline ShaderStorageBufferCreateInfo& GetShaderStorageBufferCreateInfo() { return m_CI; }

		virtual bool ResourceInUse(int timeout) = 0;
		std::function<bool(ShaderStorageBuffer*, int)> ResourceInUseCallback = &ShaderStorageBuffer::ResourceInUse;

	protected:
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		//Read/Write operations on the SSBO data.
		virtual void Access() = 0;
	};
}