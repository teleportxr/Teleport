#pragma once

#include "Skin.h"

namespace clientrender
{
	//! An instance referencing a Skin, containing live data animating a specific mesh.
	class SkinInstance
	{
	public:
		SkinInstance( std::shared_ptr<Skin> s);

		virtual ~SkinInstance() = default;

		virtual void UpdateBoneMatrices(const mat4& rootTransform);

		mat4* GetBoneMatrices(const mat4& rootTransform)
		{
			UpdateBoneMatrices(rootTransform);
			return boneMatrices;
		}
		const std::vector<std::shared_ptr<Bone>>& GetJoints() const { return joints; }
		std::shared_ptr<Skin> GetSkin()
		{
			return skin;
		}
		const std::vector<std::shared_ptr<Bone>>& GetBones() const
		{
			return bones;
		}
	protected:
		std::shared_ptr<Skin> skin;
		// TODO: this is a very crude repro of the mBoneManager locally,
		// containing only copies of the bones/joints that the original Skin has.
		// This must be made MUCH more efficient.
		std::unordered_map<avs::uid,std::shared_ptr<Bone>> boneMap;
		std::vector<std::shared_ptr<Bone>> bones;
		std::vector<std::shared_ptr<Bone>> joints;
		//Internal function for returning the bone matrices without updating them.
		mat4* GetBoneMatrices()
		{
			return boneMatrices;
		}
		mat4 boneMatrices[Skin::MAX_BONES];
	};
}
