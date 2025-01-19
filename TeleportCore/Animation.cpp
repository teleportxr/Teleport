#include "TeleportCore/Animation.h"
#include "TeleportCore/Logging.h"
#include "TeleportCore/ErrorHandling.h"

using namespace teleport;
using namespace core;

bool TransformKeyframeList::operator == (const TransformKeyframeList &t) const
{
	VERIFY_EQUALITY_CHECK(t, boneIndex);
	VERIFY_EQUALITY_CHECK(t, positionKeyframes.size())
	for (size_t i = 0; i < t.positionKeyframes.size(); i++)
	{
		VERIFY_EQUALITY_CHECK(t, positionKeyframes[i].time)
		VERIFY_EQUALITY_CHECK(t, positionKeyframes[i].value)
	}
	VERIFY_EQUALITY_CHECK(t, rotationKeyframes.size())
	for (size_t i = 0; i < t.rotationKeyframes.size(); i++)
	{
		VERIFY_EQUALITY_CHECK(t, rotationKeyframes[i].time)
		VERIFY_EQUALITY_CHECK(t, rotationKeyframes[i].value)
	}
	return true;
}


bool Animation::Verify(const Animation &t) const
{
	VERIFY_EQUALITY_CHECK(t, boneKeyframes.size());
	VERIFY_EQUALITY_CHECK(t, duration);
	for (size_t i = 0; i < t.boneKeyframes.size(); i++)
	{
		VERIFY_EQUALITY_CHECK(t, boneKeyframes[i]);
	}
	return true;
}

TransformKeyframeList TransformKeyframeList::convertToStandard(const TransformKeyframeList &keyframeList, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
{
	TransformKeyframeList convertedKeyframeList = keyframeList;

	for (Vector3Keyframe &vectorKeyframe : convertedKeyframeList.positionKeyframes)
	{
#if TELEPORT_INTERNAL_CHECKS
		if (_isnanf(vectorKeyframe.value.x) || _isnanf(vectorKeyframe.value.y) || _isnanf(vectorKeyframe.value.z) || _isnanf(vectorKeyframe.time))
		{
			TELEPORT_CERR << "Invalid keyframe" << std::endl;
			return convertedKeyframeList;
		}
#endif
		avs::ConvertPosition(sourceStandard, targetStandard, vectorKeyframe.value);
	}

	for (Vector4Keyframe &vectorKeyframe : convertedKeyframeList.rotationKeyframes)
	{
#if TELEPORT_INTERNAL_CHECKS
		if (_isnanf(vectorKeyframe.value.x) || _isnanf(vectorKeyframe.value.y) || _isnanf(vectorKeyframe.value.z) || _isnanf(vectorKeyframe.value.w) || _isnanf(vectorKeyframe.time))
		{
			TELEPORT_CERR << "Invalid keyframe" << std::endl;
			return convertedKeyframeList;
		}
#endif
		avs::ConvertRotation(sourceStandard, targetStandard, vectorKeyframe.value);
	}

	return convertedKeyframeList;
}