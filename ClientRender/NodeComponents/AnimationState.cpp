#include "AnimationState.h"
#include <chrono>
#include <cmath>
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

//! Duration of the clip a state refers to, or zero if it has none.
//! A state whose clip has not streamed in yet holds a null animation - SetAnimationState stores the
//! state anyway, so that it applies once the clip arrives - and a clip whose duration never got
//! filled in is zero. Every site here already treats a zero duration as "no timeline to advance
//! along: leave the ratio where it is", so folding null into the same case keeps one code path and,
//! more to the point, means nothing here dereferences a clip that isn't there.
static float ClipDuration(const AnimationState &st)
{
	return st.animation ? st.animation->duration : 0.0f;
}

//! The phase-matched ratio at the end of an interval across which the clip duration changes linearly
//! from d0 to d1, starting from ratio R0. Integrating dR/dt = 1/d(t) gives the logarithm; when the
//! duration does not change (r == 0) that degenerates to the linear form, and when either duration is
//! zero there is no phase to match, so the ratio stands still rather than becoming an infinity that
//! turns into a NaN the moment getState() wraps it into the unit interval.
static float PhaseMatchedRatio(float R0, float d0, float d1, float dt, float elapsed)
{
	if (d0 <= 0.0f)
	{
		return R0;
	}
	const float r = (d1 > 0.0f && dt != 0.0f) ? ((d1 - d0) / dt) : 0.0f;
	if (r > 0.0f)
	{
		const float scaled = (elapsed * r + d0) / d0;
		if (scaled > 0.0f)
		{
			return R0 + std::log(scaled) / r;
		}
	}
	return R0 + elapsed / d0;
}

//! Copy a state and advance it to the query time. The stored timeRatio is the base that elapsed
//! playback is added to, not something to throw away: discarding it restarts the clip from its first
//! frame on every ApplyAnimation the server sends.
static void ApplyExtrapolated(AnimationState &out, const AnimationState &in, int64_t timestampUs)
{
	out			  = in;
	const float d = ClipDuration(in);
	const float t = in.speedUnitsPerS * UsToS(in.timestampUs, timestampUs);
	out.timeRatio = in.timeRatio + (d ? (t / d) : 0.0f);
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
				// The snapshot is the current state wound *forwards* to now, so the cross-fade starts from
				// where playback actually is. Elapsed time is measured from the state being snapshotted -
				// measuring it from the new, future state instead runs the clip backwards, and every
				// transition then starts from a phase the clip has already passed.
				AnimationState snapshot;
				ApplyExtrapolated(snapshot, last->second, time_now_us);
				snapshot.timestampUs			= time_now_us;
				animationStates[time_now_us]	= snapshot;
				sequenceNumber					+=1000;
			}
		}
	}
	sequenceNumber++;
	if(st.matchTransition&&animationStates.size()>0)
	{
		// Copied out, not referenced: inserting the new state below can alias the last one when the two
		// share a timestamp, and the assignment would then overwrite what we are reading from.
		const AnimationState lastState	= animationStates.rbegin()->second;
		const float			 d0			= ClipDuration(lastState);
		if(d0 > 0)
		{
			auto &state		= animationStates[st.timestampUs];
			state			= st;
			// Carry the footfall across the change of clip: the ratio at the new state continues from
			// the old one rather than restarting.
			state.timeRatio = PhaseMatchedRatio(lastState.timeRatio, d0, ClipDuration(state), UsToS(lastState.timestampUs, state.timestampUs),
											   UsToS(lastState.timestampUs, state.timestampUs));
			return;
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
		ApplyExtrapolated(st.animationState, s1->second, timestampUs);
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
			ApplyExtrapolated(st.animationState, animationState, timestampUs);
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
		ApplyExtrapolated(st.animationState, s1->second, timestampUs);
		interpState							= 4;
		return st;
	}
	int64_t interpolation_time_us = timestampUs - s0->first;
	st.interpolation = float(double(interpolation_time_us) / double(s1->first - s0->first));
	const AnimationState &animationState0		= s0->second;
	const AnimationState &animationState1		= s1->second;
	// Each side of the blend runs on its own clip's phase, advanced from its own stored base since its
	// own timestamp. Crossing the two over - reading the outgoing clip's phase on the incoming state -
	// is what makes a transition visibly jump.
	ApplyExtrapolated(st.previousAnimationState, animationState0, timestampUs);
	ApplyExtrapolated(st.animationState, animationState1, timestampUs);
	if (s0 != animationStates.begin())
		animationStates.erase(animationStates.begin());
	interpState = 5;
	// A phase-matched transition, e.g. walking into running, keeps its footfall: both sides run on one
	// shared phase, integrated across the duration changing from the outgoing clip's to the incoming
	// one's. Without it the two clips are independent and each keeps the phase computed above.
	if(st.animationState.matchTransition)
	{
		const float elapsed = animationState0.speedUnitsPerS * UsToS(animationState0.timestampUs, timestampUs);
		const float R		= PhaseMatchedRatio(animationState0.timeRatio, ClipDuration(animationState0), ClipDuration(animationState1),
											   UsToS(s0->first, s1->first), elapsed);
		st.previousAnimationState.timeRatio	= R;
		st.animationState.timeRatio			= R;
	}
	return st;
}
