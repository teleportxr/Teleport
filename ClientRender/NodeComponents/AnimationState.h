#pragma once

#include <memory>

#include "ClientRender/Animation.h"
#include <ozz/animation/runtime/sampling_job.h>
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/endianness.h"

#define tinygltf teleport_tinygltf
#include "tiny_gltf.h"
#include "ozz/gltf2ozz.h"

namespace teleport
{
	namespace clientrender
	{
		struct PlaybackController
		{
			// Sets animation current time.
			// Returns the number of loops that happened during update. A positive numbre
			// means looping going foward, a negative number means looping going backward.
			int set_time_ratio(float _ratio)
			{
				//  Number of loops completed within _ratio, possibly negative if going
				//  backward.
				previous_time_ratio_ = time_ratio_;
				if (loop_)
				{
					// Wraps in the unit interval [0:1]
					const float loops = floorf(_ratio);
					time_ratio_		  = _ratio - loops;
					return static_cast<int>(loops);
				}
				else
				{
					// Clamps in the unit interval [0:1].
					time_ratio_ = ozz::math::Clamp(0.f, _ratio, 1.f);
					return 0;
				}
			}
			// Gets animation current time.
			float time_ratio() const
			{
				return time_ratio_;
			}
			// Gets animation time ratio of last update. Useful when the range between
			// previous and current frame needs to pe processed.
			float previous_time_ratio() const
			{
				return previous_time_ratio_;
			}
			// Sets playback speed.
			void set_playback_speed(float _speed)
			{
				playback_speed_ = _speed;
			}
			// Gets playback speed.
			float playback_speed() const
			{
				return playback_speed_;
			}
			// Sets loop modes. If true, animation time is always clamped between 0 and 1.
			void set_loop(bool _loop)
			{
				loop_ = _loop;
			}
			// Gets loop mode.
			bool loop() const
			{
				return loop_;
			}
			// Get if animation is playing, otherwise it is paused.
			bool playing() const
			{
				return play_;
			}
			// Updates animation time if in "play" state, according to playback speed and
			// given frame time dt_s.
			// Returns the number of loops that happened during update. A positive number
			// means looping going foward, a negative number means looping going backward.
			int Update(const ozz::animation::Animation &_animation, float dt_s)
			{
				float new_ratio = time_ratio_;
				if (play_)
				{
					new_ratio = time_ratio_ + dt_s * playback_speed_ / _animation.duration();
				}
				// Must be called even if time doesn't change, in order to update previous
				// frame time ratio. Uses set_time_ratio function in order to update
				// previous_time_ and wrap time value in the unit interval (depending on loop
				// mode).
				return set_time_ratio(new_ratio);
			}

		private:
			// Current animation time ratio, in the unit interval [0,1], where 0 is the
			// beginning of the animation, 1 is the end.
			float time_ratio_		   = 0.0f;
			// Time ratio of the previous update.
			float previous_time_ratio_ = 0.0f;
			// Playback speed, can be negative in order to play the animation backward.
			float playback_speed_	   = 1.0f;
			// Animation play mode state: play/pause.
			bool play_				   = true;
			// Animation loop mode.
			bool loop_				   = true;
		};


		// Helper object that manages motion accumulation of delta motions/moves to
		// compute character's transform.
		struct MotionDeltaAccumulator
		{
			//  Accumulates motion delta to updates current transform.
			void Update(const ozz::math::Transform &_delta);

			//  Accumulates motion delta to updates current transform.
			// _delta_rotation is the rotation to apply to deform the path since last
			// update. Hence, user is responsible for taking care of applying delta time
			// if he wants to achieve a specific angular speed.
			void Update(const ozz::math::Transform &_delta, const ozz::math::Quaternion &_rotation);

			// Teleports accumulator to a new origin.
			void Teleport(const ozz::math::Transform &_origin);

			// Character's current transform.
			ozz::math::Transform current		 = ozz::math::Transform::identity();

			// Delta transformation between last and current frame.
			ozz::math::Transform delta			 = ozz::math::Transform::identity();

			// Accumulated rotation (since last teleport).
			ozz::math::Quaternion rotation_accum = ozz::math::Quaternion::identity();
		};

		// Helper object that manages motion accumulation to compute character's
		// transform. Delta motion is automatically computed from the difference between
		// the last and the new transform.
		struct MotionAccumulator : public MotionDeltaAccumulator
		{
			//  Computes motion delta (new - last) and updates current transform.
			void Update(const ozz::math::Transform &_new);

			//  Accumulates motion delta (new - last) and updates current transform.
			// _delta_rotation is the rotation to apply to deform the path since last
			// update. Hence, user is responsible for taking care of applying delta time
			// if he wants to achieve a specific angular speed.
			void Update(const ozz::math::Transform &_new, const ozz::math::Quaternion &_rotation);

			// Tells the accumulator that the _new transform is the new origin.
			// This is useful when animation loops, so next delta is computed from the new
			// origin.
			void ResetOrigin(const ozz::math::Transform &_origin);

			// Teleports accumulator to a new transform. This also resets the origin, so
			// next delta is computed from the new origin.
			void Teleport(const ozz::math::Transform &_origin);

			// Last value sample from the motion track, used to compute delta.
			ozz::math::Transform last = ozz::math::Transform::identity();
		};

		struct AnimationSampler
		{
			// Buffers for local transforms and joint weights
			std::vector<ozz::math::SoaTransform> locals;
			std::vector<ozz::math::SimdFloat4>	 joint_weights;

			// Simple controller - you might want to expand this
			struct Controller
			{
				float currentTimeRatio = 0.0f;

				void  Update(const ozz::animation::Animation &animation, float dt_s)
				{
					// Simple time update - you can make this more sophisticated
					if (animation.duration() > 0.0f)
					{
						currentTimeRatio += dt_s / animation.duration();
						if (currentTimeRatio > 1.0f)
						{
							currentTimeRatio = fmodf(currentTimeRatio, 1.0f);
						}
					}
				}

				float time_ratio() const
				{
					return currentTimeRatio;
				}
			} controller;
		};
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
			avs::uid				   animationId	  = 0;
			int64_t					   timestampUs	  = 0;
			float					   animationTimeS = 0.0f;
			float					   speedUnitsPerS = 1.0f;
			bool					   loop			  = true;
			std::shared_ptr<Animation> animation;
		};
		struct InstantaneousAnimationState
		{
			AnimationState previousAnimationState;
			AnimationState animationState;
			float		   interpolation = 0.0f;
		};
		//! Manages the state of a specific animation layer applied to a specific skeleton.
		//! AnimationLayerStateSequence contains zero or more AnimationStates.
		//! An AnimationState specifies the state at a given timestamp.
		class AnimationLayerStateSequence
		{
		public:
			AnimationLayerStateSequence();
			void						Init(int num_soa_joints, int num_joints);
			//! Add a state to the sequence.
			void						AddState(std::chrono::microseconds timestampUs, const AnimationState &st);
			InstantaneousAnimationState getState(int64_t timestampUs) const;
			InstantaneousAnimationState getState() const;

			uint32_t					sequenceNumber = 0;
			mutable int					interpState	   = 0;
			AnimationSampler			sampler;

		private:
			int64_t									  lastUpdateTimestamp = 0;
			mutable std::map<int64_t, AnimationState> animationStates;
			// Interpolated data:
			mutable InstantaneousAnimationState instantaneousAnimationState;
		};
	}
}