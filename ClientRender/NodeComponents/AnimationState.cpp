#include "AnimationState.h"
#include <chrono>
using namespace teleport::clientrender;

/*static float AnimTimeAtTimestamp(const AnimationState &animationState, int64_t timestampNowUs)
{
	float timeS = animationState.animationTimeS;
	timeS += float(double(timestampNowUs - animationState.timestampUs) / 1000000.0);
	return timeS;
}*/
static float UsToS(int64_t timestampUs)
{
	return float(double(timestampUs) / 1000000.0);
}
static float UsToS(int64_t startTimestampUs,int64_t endTimestampUs)
{
	return float(double(endTimestampUs-startTimestampUs) / 1000000.0);
}


AnimationLayerStateSequence::AnimationLayerStateSequence()
{
}

void AnimationLayerStateSequence::Init(int num_soa_joints, int num_joints)
{
    // Allocates runtime buffers.
    sampler.locals.resize(num_soa_joints);
    // Allocates a context that matches animation requirements.
    //sampler.context.Resize(num_joints);
}

void AnimationLayerStateSequence::AddState(std::chrono::microseconds timestampUs, const AnimationState &st)
{
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
			//if the new state starts later then the last, and later than now,
			//create an intermediate state, a snapshot of the current state, from which to interpolate.
			if (st.timestampUs > last->first && st.timestampUs > time_now_us)
			{
				AnimationState &intermediate	= animationStates[time_now_us];
				const AnimationState &previous	= last->second;
				intermediate.animationId		= previous.animationId;
				float d							= previous.animation->duration;
				intermediate.timeRatio			= d ? (previous.timeRatio + float((time_now_us-st.timestampUs) / 1000000.0) * previous.speedUnitsPerS / d) : 0.0f;
				intermediate.loop				= previous.loop;
				intermediate.speedUnitsPerS		= previous.speedUnitsPerS;
				intermediate.timestampUs		= time_now_us;
				intermediate.animation			= previous.animation;
				sequenceNumber					+=1000;
			}
		}
	}
	sequenceNumber++;
	if(st.matchTransition&&animationStates.size()>0)
	{
		auto &lastState=animationStates.rbegin().operator*().second;
		float d0 = lastState.animation->duration;
		if(d0 > 0)
		{
			float R0 = lastState.timeRatio;
			auto &state=animationStates[st.timestampUs];
			state=st;
			if(state.timestampUs > lastState.timestampUs)
			{
				float d1 = state.animation->duration;
				// correct time ratio at new state?
				float dt = UsToS(lastState.timestampUs,state.timestampUs);
				float r = (d1 - d0) / (dt);
				if (r > 0)
				{
					float R1 = R0 + std::log(d1 / d0) / r;
					state.timeRatio = R1;
					return;
				}
			}
			else
			{
				float d1 = state.animation->duration;
				// correct time ratio at new state?
				float dt = UsToS(lastState.timestampUs,state.timestampUs);
				float r = (d1 - d0) / (dt);
				if (r > 0)
				{
					float R1 = R0 + std::log(d1 / d0) / r;
					state.timeRatio = R1;
					return;
				}
			}
		}
	}
	auto &state=animationStates[st.timestampUs];
	state=st;
}

const InstantaneousAnimationState &AnimationLayerStateSequence::getState() const
{
	return instantaneousAnimationState;
}

const InstantaneousAnimationState &AnimationLayerStateSequence::getState(int64_t timestampUs) const
{
	InstantaneousAnimationState &st = getStateInternal(timestampUs);
	if(std::isnan(st.previousAnimationState.timeRatio))
	{
		getStateInternal(timestampUs);
	}
	if(std::isnan(st.animationState.timeRatio))
	{
		getStateInternal(timestampUs);

	}
	const float loops0					= floorf(st.animationState.timeRatio);
	st.animationState.timeRatio			-= loops0;
	const float loops1					= floorf(st.previousAnimationState.timeRatio);
	st.previousAnimationState.timeRatio	-= loops1;
	return st;
}

InstantaneousAnimationState &AnimationLayerStateSequence::getStateInternal(int64_t timestampUs) const
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
		st.interpolation					= 1.0f;
		st.animationState.animationId		= s1->second.animationId;
		st.animationState.speedUnitsPerS	= s1->second.speedUnitsPerS;
		st.animationState.timestampUs		= s1->second.timestampUs;
		st.animationState.loop				= s1->second.loop;
		st.animationState.matchTransition	= s1->second.matchTransition;
		st.animationState.animation			= s1->second.animation;
		float d								= s1->second.animation->duration;
		float t								= s1->second.speedUnitsPerS * UsToS(s1->second.timestampUs, timestampUs);
		st.animationState.timeRatio			= s1->second.timeRatio + d?(t / d):0.0f;
		interpState							= 1;
		return st;
	}
	// If all values are before this timestamp, use the endmost value.
	if (s1 == animationStates.end())
	{
		auto s_last = animationStates.rbegin();
		if (s_last != animationStates.rend())
		{
			const AnimationState &animationState= s_last->second;
			st.interpolation					= 1.0f;
			st.animationState.animationId		= animationState.animationId;
			float t								= s_last->second.speedUnitsPerS * UsToS(s_last->second.timestampUs, timestampUs);
			st.animationState.speedUnitsPerS	= animationState.speedUnitsPerS;
			st.animationState.timestampUs		= animationState.timestampUs;
			st.animationState.loop				= animationState.loop;
			st.animationState.matchTransition	= animationState.matchTransition;
			st.animationState.animation			= animationState.animation;
			float d								= animationState.animation->duration;
			st.animationState.timeRatio			= s_last->second.timeRatio + d?(t / d):0.0f;
			st.previousAnimationState			= animationState;
			if (animationStates.size() > 1)
			{
				animationStates.erase(animationStates.begin());
			}
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
		st.interpolation					= 1.0f;
		st.animationState.animationId		= s1->second.animationId;
		st.animationState.speedUnitsPerS	= s1->second.speedUnitsPerS;
		st.animationState.timestampUs		= s1->second.timestampUs;
		st.animationState.loop				= s1->second.loop;
		st.animationState.matchTransition	= s1->second.matchTransition;
		st.animationState.animation			= s1->second.animation;
		float d								= s1->second.animation->duration;
		float t								= s1->second.speedUnitsPerS * UsToS(s1->second.timestampUs, timestampUs);
		st.animationState.timeRatio			= s1->second.timeRatio + d?(t / d):0.0f;
		interpState							= 4;
		return st;
	}
	int64_t interpolation_time_us = timestampUs - s0->first;
	st.interpolation = float(double(interpolation_time_us) / double(s1->first - s0->first));
	const AnimationState &animationState0		= s0->second;
	st.previousAnimationState.animationId		= animationState0.animationId;
	st.previousAnimationState.speedUnitsPerS	= animationState0.speedUnitsPerS;
	st.previousAnimationState.timestampUs		= animationState0.timestampUs;
	
	float t										= animationState0.speedUnitsPerS * UsToS(animationState0.timestampUs, timestampUs);
	const AnimationState &animationState1		= s1->second;
	st.animationState.animationId				= animationState1.animationId;
	st.animationState.speedUnitsPerS			= animationState1.speedUnitsPerS;
	st.animationState.timestampUs				= animationState1.timestampUs;
	st.animationState.loop						= animationState1.loop;
	st.animationState.matchTransition			= animationState1.matchTransition;
	st.animationState.animation					= animationState1.animation;
	// In the case that we have two usable keyframes, and they represent the same animation, we must interpolate the animationTime, rather than extrapolating it.
	if (st.previousAnimationState.animationId == st.animationState.animationId)
	{
		//st.previousAnimationState.animationTimeS	= lerp(st.previousAnimationState.animationTimeS, st.animationState.animationTimeS, st.interpolation);
		//st.animationState.animationTimeS			= lerp(st.previousAnimationState.animationTimeS, st.animationState.animationTimeS, st.interpolation);
	}
	if (s0 != animationStates.begin())
		animationStates.erase(animationStates.begin());
	interpState = 5;
	if(st.animationState.matchTransition)
	{
		float d0			= animationState0.animation->duration;
		float d1			= animationState1.animation->duration;
		float t0			= 0;
		float t1			= UsToS(s1->first-s0->first);
		float R0 = animationState0.timeRatio;
		float R1 = animationState1.timeRatio;
		float r				= (d1-d0)/(t1-t0);
		float R;
		if(r>0)
		{
			R				= R0 + log(((t-t0)*r+d0)/d0)/r;
		}
		else
		{
			R = R0 + d0 ? (t / d0):0.0f;
		}
		st.previousAnimationState.timeRatio	= R;
		st.animationState.timeRatio			= R;
	}
	else // anims are independent.
	{
		float t0							= animationState0.speedUnitsPerS * UsToS(animationState0.timestampUs, timestampUs);
		float d0							= animationState0.animation->duration;
		st.animationState.timeRatio			= d0?(t / d0):0.0f;
		float t1							= animationState1.speedUnitsPerS * UsToS(animationState1.timestampUs, timestampUs);
		float d1							= animationState1.animation->duration;
		st.previousAnimationState.timeRatio	= d1?(t1 / d1) : 0.0f;
	}
	return st;
}
