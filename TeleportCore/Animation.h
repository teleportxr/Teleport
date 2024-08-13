#pragma once

#include "libavstream/common.hpp"
#include "libavstream/geometry/mesh_interface.hpp"
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
			int16_t boneIndex = -1; //Index of the bone used in the bones list.

			std::vector<Vector3Keyframe> positionKeyframes;
			std::vector<Vector4Keyframe> rotationKeyframes;
			bool operator!=(const TransformKeyframeList &t) const
			{
				return !(operator==(t));
			}
			bool operator==(const TransformKeyframeList &t) const;
			template <typename OutStream>
			friend OutStream &operator<<(OutStream &out, const TransformKeyframeList &k)
			{
				out.writeChunk(k.boneIndex);
				out.writeChunk(k.positionKeyframes.size());
				for (size_t i = 0; i < k.positionKeyframes.size(); i++)
				{
					out.writeChunk(k.positionKeyframes[i].time);
					out.writeChunk(k.positionKeyframes[i].value.x);
					out.writeChunk(k.positionKeyframes[i].value.y);
					out.writeChunk(k.positionKeyframes[i].value.z);
				}
				out.writeChunk(k.rotationKeyframes.size());
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
				in.readChunk(k.boneIndex);
				size_t n=0;
				in.readChunk(n);
				size_t smallest_size_remaining = n / sizeof(Vector3Keyframe);
				if (smallest_size_remaining > in.getBytesRemaining())
				{
					std::cerr<< "Bad file " << in.filename << "\n";
					return in;
				}
				k.positionKeyframes.resize(n);
				for (size_t i = 0; i < n; i++)
				{
					in.readChunk(k.positionKeyframes[i].time);
					in.readChunk(k.positionKeyframes[i].value.x);
					in.readChunk(k.positionKeyframes[i].value.y);
					in.readChunk(k.positionKeyframes[i].value.z);
				}
				in.readChunk(n);
				smallest_size_remaining = n / sizeof(Vector4Keyframe);
				if (smallest_size_remaining > in.getBytesRemaining())
				{
					std::cerr << "Bad file " << in.filename << "\n";
					return in;
				}
				k.rotationKeyframes.resize(n);
				for (size_t i = 0; i < n; i++)
				{
					in.readChunk(k.rotationKeyframes[i].time);
					in.readChunk(k.rotationKeyframes[i].value.x);
					in.readChunk(k.rotationKeyframes[i].value.y);
					in.readChunk(k.rotationKeyframes[i].value.z);
					in.readChunk(k.rotationKeyframes[i].value.w);
				}
				return in;
			}
			static TransformKeyframeList convertToStandard(const TransformKeyframeList& keyframeList, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard);
		};

		//! An animation, comprising a list of keyframes.
		struct Animation
		{
			std::string name;
			float duration;
			std::vector<TransformKeyframeList> boneKeyframes;
			static const char *fileExtension()
			{
				return "teleport_animation";
			}
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
			bool Verify(const Animation &t) const;
			template <typename OutStream>
			friend OutStream &operator<<(OutStream &out, const Animation &animation)
			{
				out << animation.name;
				out.writeChunk(animation.duration);
				uint16_t n=animation.boneKeyframes.size();
				out.writeChunk(n);
				for(size_t i=0;i<animation.boneKeyframes.size();i++)
				{
					out<<animation.boneKeyframes[i];
				}
				return out;
			}
			template <typename InStream>
			friend InStream &operator>>(InStream &in, Animation &animation)
			{
				if (in.filename.rfind(".teleport_anim") != in.filename.length() - strlen(".teleport_anim"))
				{
					std::cerr<<  "Unknown animation file format for " << in.filename << "\n";
					return in;
				}
				in >> animation.name;
				in.readChunk(animation.duration);
				uint16_t n=0;
				in.readChunk(n);
				size_t smallest_size_remaining=n/sizeof(TransformKeyframeList);
				if(smallest_size_remaining>in.getBytesRemaining())
				{
					std::cerr<< "Bad file " << in.filename << "\n";
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