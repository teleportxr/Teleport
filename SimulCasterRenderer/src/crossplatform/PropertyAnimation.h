#pragma once

#include <memory>
#include <vector>

#include "libavstream/geometry/mesh_interface.hpp"

namespace scr
{
class Actor;
class Bone;

class BoneKeyframes
{
public:
	BoneKeyframes();

	std::weak_ptr<Bone> bonePtr;

	std::vector<avs::FloatKeyframe> positionXKeyframes;
	std::vector<avs::FloatKeyframe> positionYKeyframes;
	std::vector<avs::FloatKeyframe> positionZKeyframes;

	std::vector<avs::FloatKeyframe> rotationXKeyframes;
	std::vector<avs::FloatKeyframe> rotationYKeyframes;
	std::vector<avs::FloatKeyframe> rotationZKeyframes;
	std::vector<avs::FloatKeyframe> rotationWKeyframes;

	void seekTime(float time);
private:
	void setPositionToTime(float time, float& boneProperty, const std::vector<avs::FloatKeyframe>& keyframes);
	void setRotationToTime(float time, float& boneProperty, const std::vector<avs::FloatKeyframe>& keyframes);

	size_t getNextKeyframeIndex(float time, const std::vector<avs::FloatKeyframe>& keyframes);
	float getTimeBlend(float currentTime, float previousTime, float nextTime);
};

class Animation
{
public:
	std::vector<BoneKeyframes> boneAnimations;

	Animation();
	Animation(std::vector<BoneKeyframes> boneAnimations);
	
	//Returns whether the animation has finished.
	bool finished();

	//Resets internal timer to zero.
	void restart();

	//Updates animation internal time, and sets bones to new transforms.
	void update(float deltaTime);
private:
	float currentTime = 0.0f;
	float endTime = 0.0f;
};
}