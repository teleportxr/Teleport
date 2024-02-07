#pragma once

#include "libavstream/common.hpp"
#include "libavstream/geometry/mesh_interface.hpp"
#include "TeleportCore/ErrorHandling.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"

namespace teleport
{
	namespace core
	{
		struct FloatKeyframe
		{
			float time; //Milliseconds
			float value;
		};

		struct Vector3Keyframe
		{
			float time; //Milliseconds
			vec3 value;
		};

		struct Vector4Keyframe
		{
			float time; //Milliseconds
			vec4 value;
		};

		//! A list of keyframes to be used in an animation.
		struct TransformKeyframeList
		{
			size_t boneIndex = -1; //Index of the bone used in the bones list.

			std::vector<Vector3Keyframe> positionKeyframes;
			std::vector<Vector4Keyframe> rotationKeyframes;
			bool operator!=(const TransformKeyframeList &t) const
			{
				return !(operator==(t));
			}
			bool operator==(const TransformKeyframeList &t) const
			{
				if(boneIndex!=t.boneIndex)
					return false;
				if (positionKeyframes.size() != t.positionKeyframes.size())
					return false;
				for (size_t i = 0; i < t.positionKeyframes.size(); i++)
				{
					if(positionKeyframes[i].time!=t.positionKeyframes[i].time)
						return false;
					if (positionKeyframes[i].value != t.positionKeyframes[i].value)
						return false;
				}
				if (rotationKeyframes.size() != t.rotationKeyframes.size())
					return false;
				for (size_t i = 0; i < t.rotationKeyframes.size(); i++)
				{
					if (rotationKeyframes[i].time != t.rotationKeyframes[i].time)
						return false;
					if (rotationKeyframes[i].value != t.rotationKeyframes[i].value)
						return false;
				}
				return true;
			}
			template <typename OutStream>
			friend OutStream &operator<<(OutStream &out, const TransformKeyframeList &k)
			{
				out << k.boneIndex;
				out << k.positionKeyframes.size();
				for (size_t i = 0; i < k.positionKeyframes.size(); i++)
				{
					out.writeChunk(k.positionKeyframes[i].time);
					out.writeChunk(k.positionKeyframes[i].value.x);
					out.writeChunk(k.positionKeyframes[i].value.y);
					out.writeChunk(k.positionKeyframes[i].value.z);
				}
				out << k.rotationKeyframes.size();
				for (size_t i = 0; i < k.rotationKeyframes.size(); i++)
				{
					out.writeChunk(k.rotationKeyframes[i].time);
					out.writeChunk(k.rotationKeyframes[i].value.x);
					out.writeChunk(k.rotationKeyframes[i].value.y);
					out.writeChunk(k.rotationKeyframes[i].value.z);
					out.writeChunk(k.rotationKeyframes[i].value.w);
				}
				return out;
			}
			template <typename InStream>
			friend InStream &operator>>(InStream &in, TransformKeyframeList &k)
			{
				in >> k.boneIndex;
				size_t n;
				in >> n;
				size_t smallest_size_remaining = n / sizeof(Vector3Keyframe);
				if (smallest_size_remaining > in.getBytesRemaining())
				{
					TELEPORT_CERR << "Bad file " << in.filename << "\n";
					return in;
				}
				k.positionKeyframes.resize(n);
				for (size_t i = 0; i < n; i++)
				{
					in >> k.positionKeyframes[i].time;
					in >> k.positionKeyframes[i].value.x;
					in >> k.positionKeyframes[i].value.y;
					in >> k.positionKeyframes[i].value.z;
				}
				in >> n;
				smallest_size_remaining = n / sizeof(Vector4Keyframe);
				if (smallest_size_remaining > in.getBytesRemaining())
				{
					TELEPORT_CERR << "Bad file " << in.filename << "\n";
					return in;
				}
				k.rotationKeyframes.resize(n);
				for (size_t i = 0; i < n; i++)
				{
					in >> k.rotationKeyframes[i].time;
					in >> k.rotationKeyframes[i].value.x;
					in >> k.rotationKeyframes[i].value.y;
					in >> k.rotationKeyframes[i].value.z;
					in >> k.rotationKeyframes[i].value.w;
				}
				return in;
			}
			static TransformKeyframeList convertToStandard(const TransformKeyframeList& keyframeList, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
			{
				TransformKeyframeList convertedKeyframeList = keyframeList;

				for (Vector3Keyframe& vectorKeyframe : convertedKeyframeList.positionKeyframes)
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

				for (Vector4Keyframe& vectorKeyframe : convertedKeyframeList.rotationKeyframes)
				{
#if TELEPORT_INTERNAL_CHECKS
					if (_isnanf(vectorKeyframe.value.x) || _isnanf(vectorKeyframe.value.y) || _isnanf(vectorKeyframe.value.z)
						|| _isnanf(vectorKeyframe.value.w) || _isnanf(vectorKeyframe.time))
					{
						TELEPORT_CERR << "Invalid keyframe" << std::endl;
						return convertedKeyframeList;
					}
#endif
					avs::ConvertRotation(sourceStandard, targetStandard, vectorKeyframe.value);
				}

				return convertedKeyframeList;
			}
		};

		//! An animation, comprising a list of keyframes.
		struct Animation
		{
			std::string name;
			float duration;
			std::vector<TransformKeyframeList> boneKeyframes;

			static Animation convertToStandard(const Animation& animation, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
			{
				Animation convertedAnimation;
				convertedAnimation.name = animation.name;
				convertedAnimation.duration = animation.duration;
				for (const TransformKeyframeList& keyframe : animation.boneKeyframes)
				{

					convertedAnimation.boneKeyframes.push_back(TransformKeyframeList::convertToStandard(keyframe, sourceStandard, targetStandard));
				}

				return convertedAnimation;
			}
			bool IsValid() const
			{
				return boneKeyframes.size() != 0;
			}
			bool Verify(const Animation &t) const
			{
				if(boneKeyframes.size()!= t.boneKeyframes.size())
					return false;
				if(duration!=t.duration)
					return false;
				for (size_t i = 0; i < t.boneKeyframes.size(); i++)
				{
					if(boneKeyframes[i]!=t.boneKeyframes[i])
						return false;
				}
				return true;
			}
			template <typename OutStream>
			friend OutStream &operator<<(OutStream &out, const Animation &animation)
			{
				out << animation.name;
				out.writeChunk(animation.duration);
				out << animation.boneKeyframes.size();
				for(size_t i=0;i<animation.boneKeyframes.size();i++)
				{
					out<<animation.boneKeyframes[i];
				}
				return out;
			}
			template <typename InStream>
			friend InStream &operator>>(InStream &in, Animation &animation)
			{
				if (in.filename.rfind(".teleport_anim") != in.filename.length() - 8)
				{
					TELEPORT_CERR << "Unknown animation file format for " << in.filename << "\n";
					return in;
				}
				in >> animation.name;
				in >>animation.duration;
				size_t n;
				in>>n;
				size_t smallest_size_remaining=n/sizeof(TransformKeyframeList);
				if(smallest_size_remaining>in.getBytesRemaining())
				{
					TELEPORT_CERR << "Bad file " << in.filename << "\n";
					return in;
				}
				animation.boneKeyframes.resize(n);
				for(size_t i=0;i<n;i++)
				{
					in>>animation.boneKeyframes[i];
				}
				return in;
			}
		};
	}
}