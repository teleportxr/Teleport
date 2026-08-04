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

//! Duration in seconds of the clip a state refers to, or zero if the clip never resolved -
//! a state whose animation uid was unknown, or whose clip was still in flight, is stored with
//! a null animation and must not be dereferenced.
static float DurationOf(const AnimationState &st)
{
	return st.animation ? st.animation->duration : 0.0f;
}

//! Advance a normalised time ratio by t seconds of playback of a clip lasting d seconds.
//! A zero duration (unresolved clip) leaves the ratio where it was.
static float AdvanceTimeRatio(float timeRatio, float t, float d)
{
	return d > 0.0f ? timeRatio + t / d : timeRatio;
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
				const AnimationState previous	= last->second;
				AnimationState &intermediate	= animationStates[time_now_us];
				intermediate.animationId		= previous.animationId;
				// The snapshot is the previous state advanced to now, so the elapsed time runs from
				// the previous state's own timestamp - not from the new state, which is in the future
				// and would wind the clip backwards.
				float elapsed_s					= UsToS(previous.timestampUs, time_now_us) * previous.speedUnitsPerS;
				intermediate.timeRatio			= AdvanceTimeRatio(previous.timeRatio, elapsed_s, DurationOf(previous));
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
		const AnimationState lastState = animationStates.rbegin()->second;
		float d0 = DurationOf(lastState);
		float d1 = DurationOf(st);
		if(d0 > 0 && d1 > 0)
		{
			float R0 = lastState.timeRatio;
			auto &state=animationStates[st.timestampUs];
			state=st;
			// Match the footfall across a change of clip: the phase is carried over by integrating
			// a linearly-changing duration, so a walk cycle blending into a run keeps its rhythm.
			float dt = UsToS(lastState.timestampUs,state.timestampUs);
			float r = dt != 0.0f ? (d1 - d0) / dt : 0.0f;
			if (r > 0)
			{
				state.timeRatio = R0 + std::log(d1 / d0) / r;
				return;
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
		float d								= DurationOf(s1->second);
		float t								= s1->second.speedUnitsPerS * UsToS(s1->second.timestampUs, timestampUs);
		st.animationState.timeRatio			= AdvanceTimeRatio(s1->second.timeRatio, t, d);
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
			float d								= DurationOf(animationState);
			st.animationState.timeRatio			= AdvanceTimeRatio(s_last->second.timeRatio, t, d);
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
		float d								= DurationOf(s1->second);
		float t								= s1->second.speedUnitsPerS * UsToS(s1->second.timestampUs, timestampUs);
		st.animationState.timeRatio			= AdvanceTimeRatio(s1->second.timeRatio, t, d);
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
	const float d0 = DurationOf(animationState0);
	const float d1 = DurationOf(animationState1);
	if(st.animationState.matchTransition)
	{
		// Both clips share one phase, so footfall survives the transition. Treat the duration as
		// changing linearly from d0 to d1 over the blend and integrate dt/duration(t) across it,
		// which is where the logarithm comes from.
		float t1			= UsToS(s1->first-s0->first);
		float R0			= animationState0.timeRatio;
		float r				= (t1 > 0.0f) ? (d1-d0)/t1 : 0.0f;
		float R;
		if(r > 0.0f && d0 > 0.0f && t*r + d0 > 0.0f)
		{
			R				= R0 + log((t*r+d0)/d0)/r;
		}
		else
		{
			// Constant duration (or an unresolved clip): the integral degenerates to t/d0.
			R				= AdvanceTimeRatio(R0, t, d0);
		}
		st.previousAnimationState.timeRatio	= R;
		st.animationState.timeRatio			= R;
	}
	else // anims are independent, so each runs on its own phase.
	{
		st.previousAnimationState.timeRatio	= AdvanceTimeRatio(animationState0.timeRatio, t, d0);
		float t1							= animationState1.speedUnitsPerS * UsToS(animationState1.timestampUs, timestampUs);
		st.animationState.timeRatio			= AdvanceTimeRatio(animationState1.timeRatio, t1, d1);
	}
	return st;
}
