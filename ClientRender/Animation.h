#pragma once

#include <memory>
#include <vector>

#include "libavstream/geometry/animation_interface.h"

namespace clientrender
{
class Bone;
struct quat;

class BoneKeyframeList
{
public:
	BoneKeyframeList();

	size_t boneIndex = -1; //Index of the bone used in the bones list.

	std::vector<avs::Vector3Keyframe> positionKeyframes;
	std::vector<avs::Vector4Keyframe> rotationKeyframes;

	void seekTime(std::shared_ptr<Bone> bone, float time);
private:
	void setPositionToTime(float time, avs::vec3& bonePosition, const std::vector<avs::Vector3Keyframe>& keyframes);
	void setRotationToTime(float time, quat& boneRotation, const std::vector<avs::Vector4Keyframe>& keyframes);

	size_t getNextKeyframeIndex(float time, const std::vector<avs::Vector3Keyframe>& keyframes);
	size_t getNextKeyframeIndex(float time, const std::vector<avs::Vector4Keyframe>& keyframes);
	float getTimeBlend(float currentTime, float previousTime, float nextTime);
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

	//Returns how many milliseconds long the animation is.
	float getAnimationLength();

	//Sets bone transforms to positions and rotations specified by the animation at the passed time.
	//	boneList : List of bones for the animation.
	//	time : Time the animation will use when moving the bone transforms in milliseconds.
	void seekTime(const std::vector<std::shared_ptr<clientrender::Bone>>& boneList, float time);
private:
	float endTime = 0.0f; //Milliseconds the animation lasts for.
};
}