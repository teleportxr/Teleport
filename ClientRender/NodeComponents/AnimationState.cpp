#include "AnimationState.h"

using namespace teleport::clientrender;

static float AnimTimeAtTimestamp(const AnimationState &animationState, int64_t timestampNowUs)
{
	float timeS = animationState.animationTimeS;
	timeS += animationState.speedUnitsPerS * float(double(timestampNowUs - animationState.timestampUs) / 1000000.0);
	return timeS;
}

AnimationLayerStateSequence::AnimationLayerStateSequence()
{
}

void AnimationLayerStateSequence::AddState(std::chrono::microseconds timestampUs, const AnimationState &st)
{
	auto microsecondsUTC = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
	int64_t time_now_us = timestampUs.count();
	std::map<int64_t, AnimationState>::iterator s = animationStates.upper_bound(time_now_us);
	// If the state added results in the current time being in between a previous state and the new state, the interpolation may "jump".
	// To avoid this, we add a copy of the previous state at the present timestamp.

	// Is time now past the last state?
	if(s==animationStates.end())
	{
		std::map<int64_t, AnimationState>::reverse_iterator prev=animationStates.rbegin();
		if(prev!=animationStates.rend())
		{
			auto last=animationStates.find(prev->first);
			if (st.timestampUs > last->first && st.timestampUs > time_now_us)
			{
				AnimationState &intermediate	= animationStates[time_now_us];
				const AnimationState &previous	= last->second;
				intermediate.animationId		= previous.animationId;
				intermediate.animationTimeS		= AnimTimeAtTimestamp(previous, time_now_us);
				intermediate.loop				= previous.loop;
				intermediate.speedUnitsPerS		= previous.speedUnitsPerS;
				intermediate.timestampUs		= time_now_us;
				sequenceNumber					+=1000;
			}
		}
	}
	auto &state=animationStates[st.timestampUs];
	state=st;
	sequenceNumber++;
}

template<typename T>
static float lerp(T a,T b,T t)
{
	return b*t+a*(T(1)-t);
}

InstantaneousAnimationState AnimationLayerStateSequence::getState() const
{
	return instantaneousAnimationState;
}

InstantaneousAnimationState AnimationLayerStateSequence::getState(int64_t timestampUs) const
{
	InstantaneousAnimationState &st = instantaneousAnimationState;
	interpState=0;
	if(!animationStates.size())
		return st;
	std::map<int64_t, AnimationState>::iterator s0,s1;
	s1 = animationStates.upper_bound(timestampUs);
	// not yet reached the first timestamp in the sequence. But we have no previous state.
	if (s1 == animationStates.begin() && s1 != animationStates.end())
	{
		st.interpolation=1.0f;
		st.animationState.animationId=s1->second.animationId;
		st.animationState.animationTimeS = AnimTimeAtTimestamp(s1->second,timestampUs);
		st.animationState.speedUnitsPerS=s1->second.speedUnitsPerS;
		st.animationState.timestampUs = s1->second.timestampUs;
		st.animationState.loop = s1->second.loop;
		interpState = 1;
		return st;
	}
	// If all values are before this timestamp, use the endmost value.
	if (s1 == animationStates.end())
	{
		auto s_last = animationStates.rbegin();
		if (s_last != animationStates.rend())
		{
			const AnimationState &animationState = s_last->second;
			st.interpolation = 1.0f;
			st.animationState.animationId		= animationState.animationId;
			st.animationState.animationTimeS	= AnimTimeAtTimestamp(animationState, timestampUs);
			st.animationState.speedUnitsPerS	= animationState.speedUnitsPerS;
			st.animationState.timestampUs = animationState.timestampUs;
			st.animationState.loop = animationState.loop;
			st.previousAnimationState=animationState;
			if (animationStates.size()>1)
				animationStates.erase(animationStates.begin());
			interpState = 2;
			return st;
		}
		else
		{
			interpState = 3;
			return st;
		}
	}
	s0 = std::prev(s1);
	// If we haven't started yet.
	if (animationStates.size() == 0 || s0 == animationStates.end())
	{
		st.interpolation = 1.0f;
		st.animationState.animationId		= s1->second.animationId;
		st.animationState.animationTimeS	= AnimTimeAtTimestamp(s1->second, timestampUs);
		st.animationState.speedUnitsPerS	= s1->second.speedUnitsPerS;
		st.animationState.timestampUs = s1->second.timestampUs;
		st.animationState.loop = s1->second.loop;
		interpState = 4;
		return st;
	}
	st.interpolation= float(double(timestampUs - s0->first) / double(s1->first - s0->first));
	const AnimationState &animationState0		= s0->second;
	st.previousAnimationState.animationId		= animationState0.animationId;
	st.previousAnimationState.animationTimeS	= AnimTimeAtTimestamp(animationState0, timestampUs);
	st.previousAnimationState.speedUnitsPerS	= animationState0.speedUnitsPerS;
	st.previousAnimationState.timestampUs		= animationState0.timestampUs;
	const AnimationState &animationState1		= s1->second;
	st.animationState.animationId				= animationState1.animationId;
	st.animationState.animationTimeS			= AnimTimeAtTimestamp(animationState1, timestampUs);
	st.animationState.speedUnitsPerS			= animationState1.speedUnitsPerS;
	st.animationState.timestampUs				= animationState1.timestampUs;
	st.animationState.loop = animationState1.loop;
	// In the case that we have two usable keyframes, and they represent the same animation, we must interpolate the animationTime, rather than extrapolating it.
	if (st.previousAnimationState.animationId == st.animationState.animationId)
	{
		st.previousAnimationState.animationTimeS = lerp(st.previousAnimationState.animationTimeS, st.animationState.animationTimeS, st.interpolation);
		st.animationState.animationTimeS = lerp(st.previousAnimationState.animationTimeS, st.animationState.animationTimeS, st.interpolation);
	}
	if (s0 != animationStates.begin())
		animationStates.erase(animationStates.begin());
	interpState = 5;
	return st;
}
