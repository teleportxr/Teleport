// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "VertexBufferLayout.h"

namespace scr
{
	//Interface for VertexBuffer
	class VertexBuffer : public APIObject
	{
	protected:
		size_t m_Size;
		const void* m_Data;

		size_t m_Count = 0;
		size_t m_Stride = 0;
		std::unique_ptr<VertexBufferLayout> m_Layout;

	public:
		VertexBuffer(RenderPlatform *r) :APIObject(r) {}
		virtual ~VertexBuffer()
		{
			m_Size = 0;
			m_Data = nullptr;

			m_Count = 0;
			m_Stride = 0;
			m_Layout = nullptr;
		};

		void SetLayout(const VertexBufferLayout& layout)
		{
			m_Layout = std::make_unique<VertexBufferLayout>(layout);
			for (auto& attrib : layout.m_Attributes)
			{
				m_Stride += static_cast<size_t>(attrib.componentCount);
			}
			m_Stride *= 4;
		}

		virtual void Create(size_t size, const void* data) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
		
		inline size_t GetCount() const { return m_Count; }

		virtual bool ResourceInUse(int timeout)= 0;
		std::function<bool(VertexBuffer*, int)> ResourceInUseCallback = &VertexBuffer::ResourceInUse;
	};
}