// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../api/VertexBuffer.h"
#include "../api/IndexBuffer.h"
#include "../api/UniformBuffer.h"
#include "Material.h"
#include "basic_linear_algebra.h"

namespace scr
{
	class Object
	{
	private:
		bool m_Static;

		std::vector<VertexBuffer>& m_VBOs;
		IndexBuffer& m_IBO;
		Material& m_Material;
		
		vec3& m_Translation;
		quat& m_Rotation;
		vec3& m_Scale;
		struct ModelData
		{
			mat4 m_ModelMatrix;
		}m_ModelData;
		static bool s_UninitialisedUBO;
		std::unique_ptr<UniformBuffer> m_UBO;

	public:
		Object(bool staticModel, std::vector<VertexBuffer>& vbos, IndexBuffer& ibo, vec3& translation, quat& rotation, vec3& scale, Material& material);

		void Bind();
		void UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);

		inline const vec3& GetTranslation() const  { return m_Translation; }
		inline const quat& GetRotation() const { return m_Rotation; }
		inline const vec3& GetScale() const { return m_Scale; }
		inline size_t GetIndexBufferCount() const { return m_IBO.GetCount(); }
	};
}