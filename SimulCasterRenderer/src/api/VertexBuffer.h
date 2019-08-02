// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "VertexBufferLayout.h"

namespace scr
{
	//Interface for VertexBuffer
	class VertexBuffer
	{
	protected:
		size_t m_Size;
		const void* m_Data;
		
		size_t m_Stride;
		std::unique_ptr<VertexBufferLayout> m_Layout;

	public:
		virtual ~VertexBuffer()
		{
			m_Size = 0;
			m_Data = nullptr;

			m_Stride = 0;
			m_Layout = nullptr;
		};

		void SetLayout(const VertexBufferLayout& layout)
		{
			m_Layout = std::make_unique<VertexBufferLayout>(layout);
			for (auto& attrib : layout.m_Attributes)
			{
				m_Stride += static_cast<size_t>(attrib.compenentCount);
			}
		}

		virtual void Create(size_t size, const void* data) = 0;
		virtual void Destroy() = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;
	};
}