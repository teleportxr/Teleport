// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "ClientRender/IndexBuffer.h"
#include "ClientRender/Material.h"
#include "ClientRender/VertexBuffer.h"

namespace teleport
{
	namespace clientrender
	{
		//! A simple mapping to a subscene.
		struct SubSceneCreate
		{
			//! The uid of the asset in the containing cache.
			avs::uid uid;
			//! The uid of the subscene's local cache in the cache/server list.
			avs::uid subscene_cache_uid;
			static const char *getTypeName()
			{
				return "SubScene";
			}
		};
	//! This can either refer to a single mesh, or to a sub-scene. If sub_scene_uid!=0, the mesh data should be empty.
		class Mesh
		{
		public:
			struct MeshCreateInfo
			{
				std::string name;
				avs::uid id=0;
				avs::uid subscene_cache_uid=0;
				std::vector<std::shared_ptr<VertexBuffer>> vertexBuffers;
				std::vector<std::shared_ptr<IndexBuffer>> indexBuffers;
				std::vector<std::shared_ptr<Material::MaterialCreateInfo>> internalMaterials;

				//! The bind matrices can either be in the mesh, or externally, in a skeleton.
				std::vector<mat4> inverseBindMatrices;

				bool clockwiseFaces = true;
			};

		protected:
			MeshCreateInfo m_CI;
			std::vector<std::shared_ptr<Material>> internalMaterials;

		public:
			explicit Mesh(const MeshCreateInfo &pMeshCreateInfo);
			explicit Mesh(const SubSceneCreate &subSceneCreate);
			~Mesh();
			std::string getName() const
			{
				return m_CI.name;
			}
			static const char *getTypeName()
			{
				return "Mesh";
			}
			inline const MeshCreateInfo &GetMeshCreateInfo() const { return m_CI; }
			const std::vector<std::shared_ptr<Material>> &GetInternalMaterials() const
			{
				return internalMaterials;
			}

			//! Bytes owned solely by this mesh: its internal (unshared, not cache-registered)
			//! materials. Its vertex and index buffers are excluded - they are the same
			//! shared_ptr instances registered with, and counted by, the cache's own buffer
			//! managers.
			size_t GetMemoryBytes() const
			{
				size_t bytes = 0;
				for (const auto &m : internalMaterials)
				{
					if (m)
						bytes += m->GetMemoryBytes();
				}
				return bytes;
			}
		};
	}
}