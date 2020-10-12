#include "Animation.h"

#include "Actor.h"
#include "Bone.h"

namespace scr
{
BoneKeyframes::BoneKeyframes()
{}

void BoneKeyframes::seekTime(float time)
{
	std::shared_ptr<Bone> bone = bonePtr.lock();
	if(!bone) return;

	Transform transform = bone->GetLocalTransform();
	setPositionToTime(time, transform.m_Translation.x, positionXKeyframes);
	setPositionToTime(time, transform.m_Translation.y, positionYKeyframes);
	setPositionToTime(time, transform.m_Translation.z, positionZKeyframes);

	setRotationToTime(time, transform.m_Rotation.i, rotationXKeyframes);
	setRotationToTime(time, transform.m_Rotation.j, rotationYKeyframes);
	setRotationToTime(time, transform.m_Rotation.k, rotationZKeyframes);
	setRotationToTime(time, transform.m_Rotation.s, rotationWKeyframes);

	transform.UpdateModelMatrix(transform.m_Translation, transform.m_Rotation, transform.m_Scale);
	bone->SetLocalTransform(transform);
}

void BoneKeyframes::setPositionToTime(float time, float& boneProperty, const std::vector<avs::FloatKeyframe>& keyframes)
{
	if(keyframes.size() == 0) return;
	if(keyframes.size() == 1)
	{
		boneProperty = keyframes[0].value;
		return;
	}

	size_t nextKeyframeIndex = getNextKeyframeIndex(time, keyframes);
	const avs::FloatKeyframe& previousKeyframe = (nextKeyframeIndex == 0 ? keyframes[nextKeyframeIndex] : keyframes[nextKeyframeIndex - 1]);
	const avs::FloatKeyframe& nextKeyframe = keyframes[nextKeyframeIndex];

	//Linear interpolation between previous keyframe and next keyframe.
	float timeBlend = getTimeBlend(time, previousKeyframe.time, nextKeyframe.time);
	boneProperty = (1 - timeBlend) * previousKeyframe.value + timeBlend * nextKeyframe.value;
}

void BoneKeyframes::setRotationToTime(float time, float& boneProperty, const std::vector<avs::FloatKeyframe> & keyframes)
{
	if(keyframes.size() == 0) return;
	if(keyframes.size() == 1)
	{
		boneProperty = keyframes[0].value;
		return;
	}

	size_t nextKeyframeIndex = getNextKeyframeIndex(time, keyframes);
	const avs::FloatKeyframe& previousKeyframe = (nextKeyframeIndex == 0 ? keyframes[nextKeyframeIndex] : keyframes[nextKeyframeIndex - 1]);
	const avs::FloatKeyframe& nextKeyframe = keyframes[nextKeyframeIndex];

	//Linear interpolation between previous keyframe and next keyframe.
	float timeBlend = getTimeBlend(time, previousKeyframe.time, nextKeyframe.time);
	boneProperty = (1 - timeBlend) * previousKeyframe.value + timeBlend * nextKeyframe.value;
}

size_t BoneKeyframes::getNextKeyframeIndex(float time, const std::vector<avs::FloatKeyframe>& keyframes)
{
	for(int i = 1; i < keyframes.size(); i++)
	{
		if(keyframes[i].time >= time) return i;
	}

	return keyframes.size() - 1;
}

float BoneKeyframes::getTimeBlend(float currentTime, float previousTime, float nextTime)
{
	return (currentTime - previousTime) / (nextTime - previousTime);
}

Animation::Animation()
{}

Animation::Animation(std::vector<BoneKeyframes> boneAnimations)
	:boneAnimations(boneAnimations)
{
	//Retrieve end time from latest time in any bone animations.
	//ASSUMPTION: This works for Unity, but does it work for Unreal?
	if(boneAnimations.empty()) return;

	BoneKeyframes& boneAnimation = boneAnimations[0];
	if(!boneAnimation.positionXKeyframes.empty())
	{
		endTime = std::max(endTime, boneAnimation.positionXKeyframes[boneAnimation.positionXKeyframes.size() - 1].time);
		return;
	}

	if(!boneAnimation.positionYKeyframes.empty())
	{
		endTime = std::max(endTime, boneAnimation.positionYKeyframes[boneAnimation.positionYKeyframes.size() - 1].time);
		return;
	}

	if(!boneAnimation.positionZKeyframes.empty())
	{
		endTime = std::max(endTime, boneAnimation.positionZKeyframes[boneAnimation.positionZKeyframes.size() - 1].time);
		return;
	}


	if(!boneAnimation.rotationXKeyframes.empty())
	{
		endTime = std::max(endTime, boneAnimation.rotationXKeyframes[boneAnimation.rotationXKeyframes.size() - 1].time);
		return;
	}

	if(!boneAnimation.rotationYKeyframes.empty())
	{
		endTime = std::max(endTime, boneAnimation.rotationYKeyframes[boneAnimation.rotationYKeyframes.size() - 1].time);
		return;
	}

	if(!boneAnimation.rotationZKeyframes.empty())
	{
		endTime = std::max(endTime, boneAnimation.rotationZKeyframes[boneAnimation.rotationZKeyframes.size() - 1].time);
		return;
	}

	if(!boneAnimation.rotationWKeyframes.empty())
	{
		endTime = std::max(endTime, boneAnimation.rotationWKeyframes[boneAnimation.rotationWKeyframes.size() - 1].time);
		return;
	}
}

bool Animation::finished()
{
	return currentTime >= endTime;
}

void Animation::restart()
{
	currentTime = 0.0f;

	for(BoneKeyframes boneKeyframes : boneAnimations)
	{
		boneKeyframes.seekTime(currentTime);
	}
}

void Animation::update(float deltaTime)
{
	currentTime = std::min(currentTime + deltaTime, endTime);

	for(BoneKeyframes boneKeyframes : boneAnimations)
	{
		boneKeyframes.seekTime(currentTime);
	}
}
}