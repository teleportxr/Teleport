#include "Animation.h"

#include "Node.h"
#include "Transform.h"
using qt=platform::crossplatform::Quaternionf;
namespace teleport
{
	namespace clientrender
	{
		BoneKeyframeList::BoneKeyframeList(float dur) : duration(dur)
		{}

		void BoneKeyframeList::seekTime(std::shared_ptr<Node> bone, float time, float strength,bool loop) const
		{
			if(!bone)
			{
				return;
			}
			Transform transform = bone->GetLocalTransform();
			blendPositionToTime(time, transform.m_Translation, positionKeyframes, strength, loop);
			blendRotationToTime(time, *((qt*)&transform.m_Rotation), rotationKeyframes, strength, loop);
			transform.UpdateModelMatrix();
			bone->SetLocalTransform(transform);
		}

		void BoneKeyframeList::blendPositionToTime(float time, vec3 &bonePosition, const std::vector<teleport::core::Vector3Keyframe> &keyframes, float strength,bool loop) const
		{
			if(keyframes.size() == 0)
			{
				return;
			}

			if(keyframes.size() == 1)
			{
				bonePosition = keyframes[0].value;
				return;
			}
			KeyframePair kf = getNextKeyframeIndex(time, keyframes, loop);
			const teleport::core::Vector3Keyframe &previousKeyframe = keyframes[kf.prev];
			const teleport::core::Vector3Keyframe &nextKeyframe = keyframes[kf.next];

			//Linear interpolation between previous keyframe and next keyframe.
			vec3 newpos = (1.0f - kf.interp) * previousKeyframe.value + kf.interp * nextKeyframe.value;
			if(strength>=1.f)
			{
				bonePosition = newpos;
			}
			else if(strength>0.f)
			{
				bonePosition =lerp(bonePosition,newpos,strength);
			}
		}

		void BoneKeyframeList::blendRotationToTime(float time, platform::crossplatform::Quaternionf &boneRotation, const std::vector<teleport::core::Vector4Keyframe> &keyframes, float strength, bool loop) const
		{
			if(keyframes.size() == 0)
			{
				return;
			}

			if(keyframes.size() == 1)
			{
				boneRotation = keyframes[0].value;
				return;
			}

			KeyframePair kf= getNextKeyframeIndex(time, keyframes,loop);
			const teleport::core::Vector4Keyframe& previousKeyframe = keyframes[kf.prev];
			const teleport::core::Vector4Keyframe& nextKeyframe = keyframes[kf.next];

			//Linear interpolation between previous keyframe and next keyframe.
			platform::crossplatform::Quaternionf &previousValue = *((platform::crossplatform::Quaternionf *)&previousKeyframe.value);
			platform::crossplatform::Quaternionf &nextValue = *((platform::crossplatform::Quaternionf *)&nextKeyframe.value);
			//boneRotation = quat::Slerp(previousKeyframe.value, nextKeyframe.value, timeBlend);
			platform::crossplatform::Quaternionf newrot = previousKeyframe.value * (1.f - kf.interp) + nextKeyframe.value * kf.interp; // quat::Slerp(previousValue, nextValue, kf.interp);
			newrot = platform::crossplatform::slerp(previousValue, nextValue, kf.interp);
			newrot.MakeUnit();
			if (strength >= 1.f)
			{
				boneRotation = newrot;
			}
			else if (strength > 0.f)
			{
				boneRotation = platform::crossplatform::slerp(boneRotation, newrot, strength);
			}
		}
		template<typename U>
		BoneKeyframeList::KeyframePair BoneKeyframeList::getNextKeyframeIndex(float time, const std::vector<U> &keyframes, bool loop) const
		{
			KeyframePair p;
			uint32_t N = (uint32_t)keyframes.size();
			// If the last keyframe is right on the loop-time, discard it and interpolate to keyframe 0 instead.
			if (loop && keyframes[N - 1].time >= duration)
			{
				N--;
			}
			if(loop)
			{
				time = fmodf(time, duration);
			}
			p.next = N-1;
			for (uint32_t i = 1; i < keyframes.size(); i++)
			{
				if(keyframes[i].time >= time)
				{
					p.next = i;
					break;
				}
			}
			if (loop)
			{
				p.prev = (p.next + N - 1) % N;
			}
			else
			{
				if(p.next>0)
					p.prev = p.next-1;
				else
					p.prev=0;
			}
			float previousTime=keyframes[p.prev].time;
			float nextTime=keyframes[p.next].time;
			if(nextTime<previousTime)
				nextTime+=duration;
			if(nextTime==previousTime)
				p.interp=0.0f;
			else
				p.interp=std::max(std::min(1.0f, (time - previousTime) / (nextTime - previousTime)), 0.0f);
			return p;
		}


		Animation::Animation(const std::string& name)
			:name(name)
		{}

		Animation::Animation(const std::string& name, float dur,std::vector<BoneKeyframeList> bk)
			: name(name), duration(dur), boneKeyframeLists(bk)
		{
			updateAnimationLength();
		}
		Animation::~Animation()
		{
		}

		//Retrieve end time from latest time in any bone animations.
		//ASSUMPTION: This works for Unity, but does it work for Unreal?
		void Animation::updateAnimationLength()
		{
			if(boneKeyframeLists.empty())
			{
				return;
			}

			BoneKeyframeList& boneAnimation = boneKeyframeLists[0];
			if(!boneAnimation.positionKeyframes.empty())
			{
				endTime_s = std::max(endTime_s, boneAnimation.positionKeyframes[boneAnimation.positionKeyframes.size() - 1].time);
				return;
			}

			if(!boneAnimation.rotationKeyframes.empty())
			{
				endTime_s = std::max(endTime_s, boneAnimation.rotationKeyframes[boneAnimation.rotationKeyframes.size() - 1].time);
				return;
			}
		}

		float Animation::getAnimationLengthSeconds()
		{
			return endTime_s;
		}

		void Animation::seekTime(const std::vector<std::shared_ptr<clientrender::Node>>& boneList, float time,float strength,bool loop) const
		{
			for(BoneKeyframeList boneKeyframeList : boneKeyframeLists)
			{
				if(boneKeyframeList.boneIndex<boneList.size())
					boneKeyframeList.seekTime(boneList[boneKeyframeList.boneIndex], time,  strength,loop);
			}
		}
	}
}