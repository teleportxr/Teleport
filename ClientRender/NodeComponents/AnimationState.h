#pragma once

#include <memory>

#include "ClientRender/Animation.h"

namespace teleport
{
	namespace clientrender
	{
		class Animation;
		//! Snapshot state of an animation at a particular timestamp.
		//! When we want to apply the animations of a given layer at a specific time,
		//! we find the two AnimationStates that that time is between.
		//! Then, we calculate an interpolation *interp* based on the two AnimationState timestamps and
		//!   the current timestamp.
		//! We apply the earlier state with a weight of *(1.0-interp)* and the later state with a weight of
		//!  *interp*
		struct AnimationState
		{
			avs::uid animationId = 0;
			int64_t timestampUs = 0;
			float animationTimeS = 0.0f;
			float speedUnitsPerS = 1.0f;
			bool loop=false;
		};
		struct InstantaneousAnimationState
		{
			AnimationState previousAnimationState;
			AnimationState animationState;
			float interpolation = 0.0f;
		};
		//! Manages the state of a specific animation layer applied to a specific skeleton.
		//! AnimationLayerStateSequence contains zero or more AnimationStates.
		//! An AnimationState specifies the state at a given timestamp.
		class AnimationLayerStateSequence
		{
		public:
			AnimationLayerStateSequence();

			//! Add a state to the sequence.
			void AddState(std::chrono::microseconds timestampUs, const AnimationState &st);
			InstantaneousAnimationState getState(int64_t timestampUs) const;
			InstantaneousAnimationState getState() const;

			uint32_t sequenceNumber=0;
			mutable int interpState = 0;
		private:
			int64_t lastUpdateTimestamp = 0;
			mutable std::map<int64_t, AnimationState> animationStates;
			// Interpolated data:
			mutable InstantaneousAnimationState instantaneousAnimationState;
		};
	}
}