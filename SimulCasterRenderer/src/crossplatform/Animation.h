#pragma once

#include <memory>
#include <vector>

#include "libavstream/geometry/animation_interface.h"

namespace scr
{
class Bone;
struct quat;

class BoneKeyframe
{
public:
	BoneKeyframe();

	std::weak_ptr<Bone> bonePtr;

	std::vector<avs::Vector3Keyframe> positionKeyframes;
	std::vector<avs::Vector4Keyframe> rotationKeyframes;

	void seekTime(float time);
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
	std::vector<BoneKeyframe> boneKeyframes;

	Animation(const std::string& name);
	Animation(const std::string& name, std::vector<BoneKeyframe> boneKeyframes);
	
	//Updates how long the animations runs for.
	void updateAnimationLength();

	//Returns whether the animation has finished.
	bool finished();

	//Resets internal timer to zero, and moves bones to start transforms.
	void restart();

	//Updates animation internal time, and sets bones to new transforms.
	void update(float deltaTime);
private:
	float currentTime = 0.0f;
	float endTime = 0.0f;
};
}