#include "AnimationComponent.h"

#include <algorithm>
#include <libavstream/src/platform.hpp>

#include "ClientRender/Animation.h"
#include "TeleportCore/CommonNetworking.h"
#include "GeometryCache.h"

using namespace teleport::clientrender;
	
AnimationComponent::AnimationComponent()
{
}

AnimationComponent::AnimationComponent(const std::map<avs::uid, std::shared_ptr<Animation>> &anims)
{
	animations=anims;
}

void AnimationComponent::addAnimation(avs::uid id, std::shared_ptr<Animation> a)
{
	animations[id]=a;
	TELEPORT_LOG("Animation {0}, {1} refs",a->name,a.use_count());
}

void AnimationComponent::removeAnimation(avs::uid id)
{
	animations.erase(id);
}

void AnimationComponent::setAnimationState(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &applyAnimation)
{
	if(applyAnimation.animLayer>=32)
	{
		TELEPORT_WARN("Exceeded maximum animation layer number.");
		return;
	}
	if (applyAnimation.animLayer >= animationLayerStates.size())
		animationLayerStates.resize(applyAnimation.animLayer + 1);
	AnimationState st;
	st.animationId		=applyAnimation.animationID;
	st.animationTimeS	=applyAnimation.animTimeAtTimestamp;
	st.speedUnitsPerS	=applyAnimation.speedUnitsPerSecond;
	// The timestamp where the state applies.
	st.timestampUs=applyAnimation.timestampUs;
	st.loop=applyAnimation.loop;
	animationLayerStates[applyAnimation.animLayer].AddState(timestampUs,st);
}

void AnimationComponent::update(const std::vector<std::shared_ptr<clientrender::Node>> &boneList, int64_t timestampUs)
{
	// Early-out if we have no layers.
	if (!animationLayerStates.size())
	{
		return;
	}
	// each animation layer is applied on top of the previous one.
	for (const auto &s : animationLayerStates)
	{
		InstantaneousAnimationState state = s.getState(timestampUs);
		// This state provides:
		//	two animationStates, previous and next.
		//	an interpolation value between 0 and 1.0
		const AnimationState &s1 = state.previousAnimationState;
		const AnimationState &s2 = state.animationState;
		auto a1 = animations.find(s1.animationId);
		if(state.interpolation<1.f&&a1!=animations.end()&&a1->second)
		{
			a1->second->seekTime(boneList, s1.animationTimeS, 1.0f,s1.loop);
		}
		auto a2 = animations.find(s2.animationId);
		if (state.interpolation >0.f && a2 != animations.end() && a2->second)
		{
			a2->second->seekTime(boneList, s2.animationTimeS, state.interpolation, s2.loop);
		}
	}
}

const std::vector<AnimationLayerStateSequence> &AnimationComponent::GetAnimationLayerStates() const
{
	return animationLayerStates;
}

