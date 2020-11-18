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

//struct PropertyKeyframe
//{
//	uid nodeID = 0;
//
//	std::vector<FloatKeyframe> positionXKeyframes;
//	std::vector<FloatKeyframe> positionYKeyframes;
//	std::vector<FloatKeyframe> positionZKeyframes;
//
//	std::vector<FloatKeyframe> rotationXKeyframes;
//	std::vector<FloatKeyframe> rotationYKeyframes;
//	std::vector<FloatKeyframe> rotationZKeyframes;
//	std::vector<FloatKeyframe> rotationWKeyframes;
//
//	static PropertyKeyframe convertToStandard(const PropertyKeyframe& keyframe, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
//	{
//		PropertyKeyframe convertedKeyframe;
//		convertedKeyframe.nodeID = keyframe.nodeID;
//
//		switch(sourceStandard)
//		{
//		case avs::AxesStandard::UnityStyle:
//			switch(targetStandard)
//			{
//			case avs::AxesStandard::EngineeringStyle:
//				convertedKeyframe.positionXKeyframes = keyframe.positionXKeyframes;
//				convertedKeyframe.positionYKeyframes = keyframe.positionZKeyframes;
//				convertedKeyframe.positionZKeyframes = keyframe.positionYKeyframes;
//
//				convertedKeyframe.rotationXKeyframes = getNegatedKeyframes(keyframe.rotationXKeyframes);
//				convertedKeyframe.rotationYKeyframes = getNegatedKeyframes(keyframe.rotationZKeyframes);
//				convertedKeyframe.rotationZKeyframes = getNegatedKeyframes(keyframe.rotationYKeyframes);
//				convertedKeyframe.rotationWKeyframes = keyframe.rotationWKeyframes;
//				break;
//			case avs::AxesStandard::GlStyle:
//				convertedKeyframe.positionXKeyframes = keyframe.positionXKeyframes;
//				convertedKeyframe.positionYKeyframes = keyframe.positionYKeyframes;
//				convertedKeyframe.positionZKeyframes = getNegatedKeyframes(keyframe.positionZKeyframes);
//
//				convertedKeyframe.rotationXKeyframes = getNegatedKeyframes(keyframe.rotationXKeyframes);
//				convertedKeyframe.rotationYKeyframes = getNegatedKeyframes(keyframe.rotationYKeyframes);
//				convertedKeyframe.rotationZKeyframes = keyframe.rotationZKeyframes;
//				convertedKeyframe.rotationWKeyframes = keyframe.rotationWKeyframes;
//				break;
//			}
//			break;
//		case avs::AxesStandard::UnrealStyle:
//			//AVSLOG(Warning) << "Unimplemented source axes standard for TransformKeyframe conversion: UnrealStyle.\n";
//			break;
//		default:
//			//AVSLOG(Warning) << "Unrecognised source axes standard for TransformKeyframe conversion.\n";
//			break;
//		}
//
//		return convertedKeyframe;
//	}
//
//	static std::vector<FloatKeyframe> getNegatedKeyframes(const std::vector<FloatKeyframe>& keyframes)
//	{
//		std::vector<FloatKeyframe> negatedKeyframes;
//		for(const FloatKeyframe& keyframe : keyframes)
//		{
//			negatedKeyframes.push_back(FloatKeyframe{keyframe.time, -keyframe.value});
//		}
//		return negatedKeyframes;
//	}
//};
//
//struct PropertyAnimation
//{
//	std::vector<PropertyKeyframe> boneKeyframes;
//
//	static PropertyAnimation convertToStandard(const PropertyAnimation& animation, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
//	{
//		PropertyAnimation convertedAnimation;
//
//		for(const PropertyKeyframe& keyframe : animation.boneKeyframes)
//		{
//			convertedAnimation.boneKeyframes.push_back(PropertyKeyframe::convertToStandard(keyframe, sourceStandard, targetStandard));
//		}
//
//		return convertedAnimation;
//	}
//};

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