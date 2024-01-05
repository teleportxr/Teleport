#include "Skeleton.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

namespace teleport
{
	namespace clientrender
	{
		Skeleton::Skeleton(const std::string &name)
			: name(name)
		{
		}

		Skeleton::Skeleton(const std::string &name, size_t numBones, const Transform &skeletonTransform)
			: name(name), bones(numBones), skeletonTransform(skeletonTransform)
		{
		}

		std::shared_ptr<Bone> Skeleton::GetBoneByName(const char *txt)
		{
			for (auto b : bones)
			{
				if (b->name == txt)
					return b;
			}
			return nullptr;
		}

		void Skeleton::SetBone(size_t index, std::shared_ptr<Bone> bone)
		{
			if (index < bones.size())
			{
				bones[index] = bone;
			}
			else
			{
				TELEPORT_CERR << "ERROR: Attempted to add bone to skeleton (" << name << ") at index " << index << " greater than size " << bones.size() << "!\n";
			}
		}
	}
}