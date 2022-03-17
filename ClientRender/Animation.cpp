#include "Animation.h"

#include "Bone.h"

namespace clientrender
{
	BoneKeyframeList::BoneKeyframeList()
	{}

	void BoneKeyframeList::seekTime(std::shared_ptr<Bone> bone, float time)
	{
		if(!bone)
		{
			return;
		}

		Transform transform = bone->GetLocalTransform();
		setPositionToTime(time, transform.m_Translation, positionKeyframes);
		setRotationToTime(time, transform.m_Rotation, rotationKeyframes);

		transform.UpdateModelMatrix();
		bone->SetLocalTransform(transform);
	}

	void BoneKeyframeList::setPositionToTime(float time, avs::vec3& bonePosition, const std::vector<avs::Vector3Keyframe>& keyframes)
	{
		if(keyframes.size() == 0)
		{
			return;
		}

		if(keyframes.size() == 1)
		{
			bonePosition = keyframes[0].value;
			return;
		}

		size_t nextKeyframeIndex = getNextKeyframeIndex(time, keyframes);
		const avs::Vector3Keyframe& previousKeyframe = (nextKeyframeIndex == 0 ? keyframes[nextKeyframeIndex] : keyframes[nextKeyframeIndex - 1]);
		const avs::Vector3Keyframe& nextKeyframe = keyframes[nextKeyframeIndex];

		//Linear interpolation between previous keyframe and next keyframe.
		float timeBlend = getTimeBlend(time, previousKeyframe.time, nextKeyframe.time);
		bonePosition = (1.0f - timeBlend) * previousKeyframe.value + timeBlend * nextKeyframe.value;
	}

	void BoneKeyframeList::setRotationToTime(float time, quat& boneRotation, const std::vector<avs::Vector4Keyframe>& keyframes)
	{
		if(keyframes.size() == 0)
		{
			return;
		}

		if(keyframes.size() == 1)
		{
			boneRotation = keyframes[0].value;
			return;
		}

		size_t nextKeyframeIndex = getNextKeyframeIndex(time, keyframes);
		const avs::Vector4Keyframe& previousKeyframe = (nextKeyframeIndex == 0 ? keyframes[nextKeyframeIndex] : keyframes[nextKeyframeIndex - 1]);
		const avs::Vector4Keyframe& nextKeyframe = keyframes[nextKeyframeIndex];

		//Linear interpolation between previous keyframe and next keyframe.
		float timeBlend = getTimeBlend(time, previousKeyframe.time, nextKeyframe.time);
		boneRotation = (1.0f - timeBlend) * previousKeyframe.value + timeBlend * nextKeyframe.value;
		//boneRotation = quat::Slerp(previousKeyframe.value, nextKeyframe.value, timeBlend);
	}

	size_t BoneKeyframeList::getNextKeyframeIndex(float time, const std::vector<avs::Vector3Keyframe>& keyframes)
	{
		for(size_t i = 1; i < keyframes.size(); i++)
		{
			if(keyframes[i].time >= time)
			{
				return i;
			}
		}

		return keyframes.size() - 1;
	}

	size_t BoneKeyframeList::getNextKeyframeIndex(float time, const std::vector<avs::Vector4Keyframe>& keyframes)
	{
		for(size_t i = 1; i < keyframes.size(); i++)
		{
			if(keyframes[i].time >= time)
			{
				return i;
			}
		}

		return keyframes.size() - 1;
	}

	float BoneKeyframeList::getTimeBlend(float currentTime, float previousTime, float nextTime)
	{
		return (currentTime - previousTime) / (nextTime - previousTime);
	}

	Animation::Animation(const std::string& name)
		:name(name)
	{}

	Animation::Animation(const std::string& name, std::vector<BoneKeyframeList> bk)
		: name(name), boneKeyframeLists(bk)
	{
		updateAnimationLength();
	}

	//Retrieve end time from latest time in any bone animations.
	//ASSUMPTION: This works for Unity, but does it work for Unreal?
	void Animation::updateAnimationLength()
	{
		if(boneKeyframeLists.empty())
		{
			return;
		}

		BoneKeyframeList& boneAnimation = boneKeyframeLists[0];
		if(!boneAnimation.positionKeyframes.empty())
		{
			endTime_s = std::max(endTime_s, boneAnimation.positionKeyframes[boneAnimation.positionKeyframes.size() - 1].time);
			return;
		}

		if(!boneAnimation.rotationKeyframes.empty())
		{
			endTime_s = std::max(endTime_s, boneAnimation.rotationKeyframes[boneAnimation.rotationKeyframes.size() - 1].time);
			return;
		}
	}

	float Animation::getAnimationLengthSeconds()
	{
		return endTime_s;
	}

	void Animation::seekTime(const std::vector<std::shared_ptr<clientrender::Bone>>& boneList, float time)
	{
		for(BoneKeyframeList boneKeyframeList : boneKeyframeLists)
		{
			boneKeyframeList.seekTime(boneList[boneKeyframeList.boneIndex], time);
		}
	}
}