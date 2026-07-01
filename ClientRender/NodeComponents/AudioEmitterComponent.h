#pragma once

#include <cstdint>

#include "NodeComponents/Component.h"

namespace teleport
{
	namespace clientrender
	{
		//! An optional component of a Node that plays a server-forwarded audio stream.
		//! See docs/protocol/audio.rst (audio emitter component).
		class AudioEmitterComponent : public Component
		{
		public:
			//! Bit flags carried in the wire ``flags`` field.
			enum Flags : uint8_t
			{
				NONE = 0,
				//! When set, the emitter is spatialised from the owning node's transform.
				//! When clear, it plays at constant gain and ignores the transform.
				SPATIALISED = 1 << 0
			};

			//! Why an emitter with ``audioStreamIndex == 0`` is silent, for UI.
			enum class SilenceReason : uint8_t
			{
				NONE = 0,
				OUT_OF_RANGE = 1,
				CAP_EXCEEDED = 2,
				MUTED = 3
			};

			AudioEmitterComponent(Node &n) : Component(n)
			{
			}
			virtual ~AudioEmitterComponent()
			{
			}

			//! The stream this emitter plays, or 0 when present but currently silent.
			uint32_t audioStreamIndex = 0;

			//! Bit field of Flags.
			uint8_t flags = SPATIALISED;

			//! Why a 0 index is silent (informational).
			SilenceReason silenceReason = SilenceReason::NONE;

			//! Linear playback gain; 1.0 = unity.
			float gain = 1.0f;

			//! Distance below which no attenuation is applied (spatialised only).
			float minDistanceMetres = 1.0f;

			//! Distance beyond which the emitter is inaudible (spatialised only).
			float maxDistanceMetres = 100.0f;

			bool IsSpatialised() const
			{
				return (flags & SPATIALISED) != 0;
			}

			//! True when a stream is currently bound to this emitter.
			bool HasStream() const
			{
				return audioStreamIndex != 0;
			}
		};
	}
}
