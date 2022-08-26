#pragma once

#include <memory>
#include <vector>

#include "TeleportCore/AnimationInterface.h"

namespace clientrender
{
class Bone;
struct quat;

//! A list of keyframes, i.e. a single track for an animation. Defines the positions and rotations for one bone in a skeleton,
//! across the length of a single animation.
class BoneKeyframeList
{
public:
	BoneKeyframeList();

	size_t boneIndex = -1; //Index of the bone used in the bones list.

	std::vector<avs::Vector3Keyframe> positionKeyframes;
	std::vector<avs::Vector4Keyframe> rotationKeyframes;

	void seekTime(std::shared_ptr<Bone> bone, float time) const;
private:
	void setPositionToTime(float time, avs::vec3& bonePosition, const std::vector<avs::Vector3Keyframe>& keyframes) const;
	void setRotationToTime(float time, quat& boneRotation, const std::vector<avs::Vector4Keyframe>& keyframes) const;

	size_t getNextKeyframeIndex(float time, const std::vector<avs::Vector3Keyframe>& keyframes) const;
	size_t getNextKeyframeIndex(float time, const std::vector<avs::Vector4Keyframe>& keyframes) const;
	float getTimeBlend(float currentTime, float previousTime, float nextTime) const;
};

class Animation
{
public:
	const std::string name;
	std::vector<BoneKeyframeList> boneKeyframeLists;

	Animation(const std::string& name);
	Animation(const std::string& name, std::vector<BoneKeyframeList> boneKeyframes);
	
	//Updates how long the animations runs for by scanning boneKeyframeLists.
	void updateAnimationLength();

	//Returns how many seconds long the animation is.
	float getAnimationLengthSeconds();

	//Sets bone transforms to positions and rotations specified by the animation at the passed time.
	//	boneList : List of bones for the animation.
	//	time : Time the animation will use when moving the bone transforms in seconds.
	void seekTime(const std::vector<std::shared_ptr<clientrender::Bone>>& boneList, float time_s) const;
private:
	float endTime_s = 0.0f; //Seconds the animation lasts for.
};
}