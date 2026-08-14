// Behavioural tests for AnimationLayerStateSequence in ClientRender/NodeComponents/AnimationState.cpp.
//
// The sequence stores animation states keyed on server-session time and, when asked for
// "now", either extrapolates the endmost state or interpolates between the two states that
// straddle the query. Everything the server can express about animation - when a clip starts,
// where in the clip it should be, how a change of clip cross-fades - is decided here, so the
// invariants worth pinning are:
//
//   - a state's stored timeRatio is a base that elapsed playback is added to, not discarded;
//   - a clip that never resolved (null animation) or has zero duration yields a finite ratio;
//   - during a blend each side runs on its own clip's phase, not the other's;
//   - a state dated in the future snapshots the current state at "now" and advances it
//     forwards, so the cross-fade starts from where playback actually is.
//
// The test compiles AnimationState.cpp directly and supplies the two clientrender::Animation
// members it needs, rather than linking ClientRender (Vulkan, draco, ktx, tiny_gltf).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ClientRender/NodeComponents/AnimationState.h"

#include <chrono>
#include <cmath>

using namespace teleport::clientrender;
using Catch::Approx;

// Seam: only the constructor and destructor are needed, and only so that a shared_ptr<Animation>
// can be made. Everything the sequence reads (duration) is a public data member.
namespace teleport
{
	namespace clientrender
	{
		Animation::Animation(const std::string &n) : name(n)
		{
		}
		Animation::~Animation()
		{
		}
	}
}

namespace
{
	constexpr int64_t kOneSecondUs = 1000000;

	std::shared_ptr<Animation> MakeClip(const char *name, float duration)
	{
		auto clip	   = std::make_shared<Animation>(name);
		clip->duration = duration;
		return clip;
	}

	//! A state that applies at timestampUs, starting the clip at timeRatio and playing at rate 1.
	//! matchTransition defaults false so that each state runs on its own phase; the phase-matching
	//! path is exercised separately.
	AnimationState MakeState(int64_t timestampUs, std::shared_ptr<Animation> clip, float timeRatio, bool matchTransition = false)
	{
		AnimationState st;
		st.animationId	   = clip ? 100 : 0;
		st.timestampUs	   = timestampUs;
		st.speedUnitsPerS  = 1.0f;
		st.timeRatio	   = timeRatio;
		st.loop			   = true;
		st.matchTransition = matchTransition;
		st.animation	   = clip;
		return st;
	}

	std::chrono::microseconds Us(int64_t us)
	{
		return std::chrono::microseconds(us);
	}
}

TEST_CASE("a single state extrapolates from its stored time ratio", "[animation_state]")
{
	// One state, two seconds of clip, starting a tenth of the way in. A second later, playback
	// should be that tenth plus half the clip. The stored ratio is a base, not something to
	// throw away: reading 0.5 here would mean every ApplyAnimation silently restarted the clip.
	AnimationLayerStateSequence seq;
	auto						clip = MakeClip("walk", 2.0f);
	seq.AddState(Us(0), MakeState(0, clip, 0.1f));

	const InstantaneousAnimationState &st = seq.getState(kOneSecondUs);

	REQUIRE(st.animationState.animation == clip);
	REQUIRE(st.animationState.timeRatio == Approx(0.6f));
	REQUIRE(st.interpolation == Approx(1.0f));
}

TEST_CASE("a zero-duration clip yields a finite time ratio", "[animation_state]")
{
	// A clip whose duration never got filled in must not divide by it. The ratio is left where
	// it was; the alternative is an infinity that becomes NaN as soon as getState() wraps it
	// into the unit interval, and NaN propagates all the way into the sampling job.
	AnimationLayerStateSequence seq;
	seq.AddState(Us(0), MakeState(0, MakeClip("empty", 0.0f), 0.3f));

	const InstantaneousAnimationState &st = seq.getState(kOneSecondUs);

	REQUIRE(std::isfinite(st.animationState.timeRatio));
	REQUIRE(st.animationState.timeRatio == Approx(0.3f));
}

TEST_CASE("a state whose clip never resolved is stored and queried without crashing", "[animation_state]")
{
	// SetAnimationState stores a state even when the animation uid is unknown or the clip is
	// still in flight, so that it applies once the clip arrives. Until then its animation is
	// null and must never be dereferenced.
	AnimationLayerStateSequence seq;
	seq.AddState(Us(0), MakeState(0, nullptr, 0.25f));
	seq.AddState(Us(0), MakeState(kOneSecondUs, nullptr, 0.5f, /*matchTransition=*/true));

	const InstantaneousAnimationState &st = seq.getState(kOneSecondUs / 2);

	REQUIRE(std::isfinite(st.animationState.timeRatio));
	REQUIRE(std::isfinite(st.previousAnimationState.timeRatio));
}

TEST_CASE("during a blend each side runs on its own clip's phase", "[animation_state]")
{
	// Two independent clips, the query halfway between them. The outgoing side has been playing
	// for half a second and the incoming side starts half a second later, so their phases run in
	// opposite directions from their own stored bases. Crossing the two over - reading the
	// outgoing clip's phase on the incoming state - is what makes a transition visibly jump.
	AnimationLayerStateSequence seq;
	auto						walk = MakeClip("walk", 2.0f);
	auto						run	 = MakeClip("run", 2.0f);
	seq.AddState(Us(0), MakeState(0, walk, 0.1f));
	seq.AddState(Us(0), MakeState(kOneSecondUs, run, 0.2f));

	const InstantaneousAnimationState &st = seq.getState(kOneSecondUs / 2);

	REQUIRE(st.interpolation == Approx(0.5f));
	// Outgoing: 0.1 + 0.5s/2.0s.
	REQUIRE(st.previousAnimationState.timeRatio == Approx(0.35f));
	// Incoming: 0.2 - 0.5s/2.0s = -0.05, wrapped into the unit interval by getState().
	REQUIRE(st.animationState.timeRatio == Approx(0.95f));
}

TEST_CASE("a future-dated state snapshots the current state advanced to now", "[animation_state]")
{
	// This is the mechanism the server relies on to cross-fade: date the new state a little
	// ahead, and the sequence inserts a snapshot of what is playing right now to interpolate
	// from. The snapshot has to be the current state wound *forwards* to now. Winding it
	// backwards - measuring the elapsed time from the new, future state instead of the one
	// being snapshotted - makes every cross-fade start from a phase the clip already passed.
	AnimationLayerStateSequence seq;
	auto						walk = MakeClip("walk", 2.0f);
	auto						run	 = MakeClip("run", 1.0f);
	seq.AddState(Us(0), MakeState(0, walk, 0.0f));
	// A second of walking has elapsed; the run state applies half a second from now.
	seq.AddState(Us(kOneSecondUs), MakeState(kOneSecondUs * 3 / 2, run, 0.0f));

	const InstantaneousAnimationState &st = seq.getState(kOneSecondUs);

	// The snapshot is the walk advanced by one second of a two-second clip.
	REQUIRE(st.previousAnimationState.timeRatio == Approx(0.5f));
	REQUIRE(st.previousAnimationState.timeRatio > 0.0f);
	REQUIRE(st.interpolation == Approx(0.0f));
}

TEST_CASE("phase-matched transitions carry the time ratio across a change of clip", "[animation_state]")
{
	// With matchTransition, a walk blending into a shorter run keeps its footfall: the ratio at
	// the new state continues from the old one rather than restarting. The exact value comes from
	// integrating across a linearly-changing duration; what matters here is that it advances from
	// the previous ratio and stays finite.
	AnimationLayerStateSequence seq;
	auto						walk = MakeClip("walk", 2.0f);
	auto						run	 = MakeClip("run", 1.0f);
	seq.AddState(Us(0), MakeState(0, walk, 0.25f));
	seq.AddState(Us(0), MakeState(kOneSecondUs, run, 0.0f, /*matchTransition=*/true));

	const InstantaneousAnimationState &st = seq.getState(kOneSecondUs / 2);

	REQUIRE(std::isfinite(st.animationState.timeRatio));
	REQUIRE(st.animationState.timeRatio == Approx(st.previousAnimationState.timeRatio));
}
