// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/VertexBuffer.h"
#include "api/IndexBuffer.h"

namespace scr
{
	class Mesh
	{
	public:
		struct MeshCreateInfo
		{
			std::string name;

			std::vector<std::shared_ptr<VertexBuffer>> vb;
			std::vector<std::shared_ptr<IndexBuffer>> ib;
		};

	protected:
		MeshCreateInfo m_CI;

	public:
		Mesh(const MeshCreateInfo& pMeshCreateInfo);

		inline const MeshCreateInfo& GetMeshCreateInfo() const { return m_CI; }
	};
}