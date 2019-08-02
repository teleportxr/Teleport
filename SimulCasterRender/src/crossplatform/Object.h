// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../api/VertexBuffer.h"
#include "../api/IndexBuffer.h"
#include "../api/UniformBuffer.h"
#include "Material.h"

namespace scr
{
	class Object
	{
	private:
		bool m_Static;

		std::vector<VertexBuffer>& m_VBOs;
		IndexBuffer& m_IBO;
		UniformBuffer& m_UBO;
		Material& m_Material;
		float m_ModelMatrix[16];
		size_t m_Size = 16 * sizeof(float);;

	public:
		Object(bool staticModel, std::vector<VertexBuffer>& vbos, IndexBuffer& ibo, UniformBuffer& ubo, float modelMatrix[16], Material& material);

		void Bind();
		void UpdateModelMatrix(float modelMatrix[16]);
	};
}