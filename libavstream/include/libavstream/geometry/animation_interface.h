#pragma once

#include "libavstream/common.hpp"
#include "libavstream/geometry/mesh_interface.hpp"

namespace avs
{
struct FloatKeyframe
{
	float time; //Milliseconds
	float value;
};

struct Vector3Keyframe
{
	float time; //Milliseconds
	vec3 value;
};

struct Vector4Keyframe
{
	float time; //Milliseconds
	vec4 value;
};

struct TransformKeyframe
{
	avs::uid nodeID;

	std::vector<avs::Vector3Keyframe> positionKeyframes;
	std::vector<avs::Vector4Keyframe> rotationKeyframes;

	static TransformKeyframe convertToStandard(const TransformKeyframe& keyframe, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
	{
		TransformKeyframe convertedKeyframe = keyframe;

		for(avs::Vector3Keyframe& vectorKeyframe : convertedKeyframe.positionKeyframes)
		{
			avs::ConvertPosition(sourceStandard, targetStandard, vectorKeyframe.value);
		}

		for(avs::Vector4Keyframe& vectorKeyframe : convertedKeyframe.rotationKeyframes)
		{
			avs::ConvertRotation(sourceStandard, targetStandard, vectorKeyframe.value);
		}

		return convertedKeyframe;
	}
};

struct Animation
{
	std::string name;
	std::vector<TransformKeyframe> boneKeyframes;

	static Animation convertToStandard(const Animation& animation, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
	{
		Animation convertedAnimation;
		convertedAnimation.name = animation.name;

		for(const TransformKeyframe& keyframe : animation.boneKeyframes)
		{
			convertedAnimation.boneKeyframes.push_back(TransformKeyframe::convertToStandard(keyframe, sourceStandard, targetStandard));
		}

		return convertedAnimation;
	}
};
}