// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <map>
#include <memory>

#include <libavstream/geometry/mesh_interface.hpp>
#include "ResourceManager.h"
#include "Actor.h"

namespace scr
{
	class ActorManager
	{
	private:
		std::map<avs::uid, std::shared_ptr<Mesh>> m_Meshes;
		std::map<avs::uid, std::shared_ptr<Material>> m_Materials;
		std::map<avs::uid, std::shared_ptr<Transform>> m_Transforms;
		
		std::map<avs::uid, std::shared_ptr<Actor>> m_Actors;

	public:
		inline void AddMesh(avs::uid mesh_uid, std::shared_ptr<Mesh>& mesh) { m_Meshes[mesh_uid] = mesh; }
		inline void AddMaterial(avs::uid material_uid, std::shared_ptr<Material>& material) { m_Materials[material_uid] = material; }
		inline void AddTransform(avs::uid transform_uid, std::shared_ptr<Transform>& transform) { m_Transforms[transform_uid] = transform; }
		
		inline const std::shared_ptr<Mesh> GetMesh(avs::uid uid) const
		{
			if(m_Meshes.find(uid) != m_Meshes.end())
			{
				return m_Meshes.at(uid);
			}
			else
				return nullptr;
		}
		inline const std::shared_ptr<Material> GetMaterial(avs::uid uid) const
		{
			if(m_Materials.find(uid) != m_Materials.end())
			{
				return m_Materials.at(uid);
			}
			else
				return nullptr;
		}
		inline const std::shared_ptr<Transform> GetTransform(avs::uid uid) const
		{
			if(m_Transforms.find(uid) != m_Transforms.end())
			{
				return m_Transforms.at(uid);
			}
			else
				return nullptr;
		}

		void CreateActor(avs::uid actor_uid, Actor::ActorCreateInfo* pActorCreateInfo)
		{
			m_Actors[actor_uid] = std::make_shared<Actor>(pActorCreateInfo);
		}
	};
}