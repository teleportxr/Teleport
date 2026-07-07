// (C) Copyright 2018-2026 Simul Software Ltd
#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// From libopus; kept as an opaque pointer here so this header stays dependency-light.
struct OpusDecoder;

namespace teleport
{
	namespace audio
	{
		class AudioPlayer;

		//! Minimal 3-vector for the spatialisation helper. Keeps this module free of
		//! the client-render / platform math types so it can be unit-tested on its own.
		struct AudioVec3
		{
			float x = 0.0f, y = 0.0f, z = 0.0f;
		};

		//! Per-source linear playback gains, one per output channel.
		struct StereoGains
		{
			float left = 1.0f, right = 1.0f;
		};

		//! Constant-power stereo pan + inverse-distance attenuation for one source.
		//! ``listenerRight`` is the listener's rightward axis (unit length). Distances
		//! are in metres; defaults match the web client
		//! (teleport-web-client/src/audio/output.ts DEFAULT_REF_DISTANCE / MAX_DISTANCE).
		inline StereoGains ComputeStereoGains(const AudioVec3 &listenerPos, const AudioVec3 &listenerRight,
			const AudioVec3 &sourcePos, float refDistance = 1.0f, float maxDistance = 100.0f)
		{
			const float rx = sourcePos.x - listenerPos.x;
			const float ry = sourcePos.y - listenerPos.y;
			const float rz = sourcePos.z - listenerPos.z;
			const float dist = std::sqrt(rx * rx + ry * ry + rz * rz);

			float atten;
			if (dist >= maxDistance)
				atten = 0.0f;
			else
				atten = refDistance / std::max(dist, refDistance);	// 1.0 within refDistance, rolling off beyond
			if (atten > 1.0f)
				atten = 1.0f;

			float pan = 0.0f;	// -1 = hard left, +1 = hard right
			if (dist > 1e-6f)
			{
				pan = (rx * listenerRight.x + ry * listenerRight.y + rz * listenerRight.z) / dist;
				if (pan < -1.0f) pan = -1.0f;
				if (pan > 1.0f) pan = 1.0f;
			}

			const float angle = (pan + 1.0f) * 0.25f * 3.14159265358979323846f;	// [0, pi/2]
			StereoGains g;
			g.left = atten * std::cos(angle);
			g.right = atten * std::sin(angle);
			return g;
		}

		//! Mixes multiple server-forwarded Opus voice streams into one spatialised
		//! stereo output. Each source is identified by the emitting node's uid — the
		//! WebRTC track SDP ``mid`` (see docs/protocol/audio.rst). Opus frames arrive on
		//! the network thread (PushOpusFrame); the per-frame spatial gains are published
		//! from the render thread (SetSourceSpatial); a dedicated output thread decodes,
		//! spatialises, sums and writes stereo PCM to the AudioPlayer.
		class SpatialAudioMixer
		{
		public:
			SpatialAudioMixer();
			~SpatialAudioMixer();

			//! Begin mixing to ``player`` (must already be configured for stereo, 48 kHz, 16-bit).
			void Start(AudioPlayer *player);
			//! Stop the output thread and release all sources. Safe to call repeatedly.
			void Stop();
			bool IsRunning() const { return running.load(); }

			//! Enqueue one Opus frame for the source with this node uid (network thread).
			//! The source (and its decoder) is created on first use.
			void PushOpusFrame(uint64_t nodeUid, const uint8_t *data, size_t size);
			//! Publish the current per-channel gains for a source (render thread). No-op if unknown.
			void SetSourceSpatial(uint64_t nodeUid, float leftGain, float rightGain);
			//! Drop a source and free its decoder (e.g. when its track closes).
			void RemoveSource(uint64_t nodeUid);
			//! Snapshot of the currently-active source uids, for the render loop to position.
			std::vector<uint64_t> GetActiveSources() const;

			//! Decode + spatialise + sum one interleaved-stereo block of ``blockFrames``
			//! frames into ``out`` (size blockFrames*2). Normally driven by the output
			//! thread; exposed so the mix can be exercised synchronously in tests.
			void MixBlock(int16_t *out, int blockFrames);

		private:
			struct SourceChannel
			{
				OpusDecoder *decoder = nullptr;
				std::mutex frameMutex;
				std::queue<std::vector<uint8_t>> frameQueue;	// pending Opus frames
				std::deque<int16_t> pcm;						// decoded-but-unmixed mono PCM
				std::atomic<float> leftGain{1.0f};
				std::atomic<float> rightGain{1.0f};
				std::atomic<uint64_t> lastActiveMs{0};

				~SourceChannel();
			};

			void OutputThread();
			void DecodeInto(SourceChannel &ch);	// drain frameQueue -> ch.pcm
			std::vector<std::shared_ptr<SourceChannel>> SnapshotChannels() const;
			void ReapIdle(uint64_t nowMs);

			AudioPlayer *audioPlayer = nullptr;
			std::atomic<bool> running{false};
			std::thread outputThread;

			mutable std::mutex sourcesMutex;
			std::map<uint64_t, std::shared_ptr<SourceChannel>> sources;
		};
	}
}
