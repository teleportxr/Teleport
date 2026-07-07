// (C) Copyright 2018-2026 Simul Software Ltd
#include "SpatialAudioMixer.h"
#include "AudioPlayer.h"

#include <chrono>
#include <cstring>
#include <opus.h>

using namespace teleport::audio;

namespace
{
	// One 20 ms Opus frame at 48 kHz mono. Also the mixer's output block size.
	constexpr int kSampleRate = 48000;
	constexpr int kBlockFrames = 960;			// 20 ms @ 48 kHz
	// Opus can emit up to 60 ms per frame; size the scratch buffer for the worst case.
	constexpr int kMaxDecodeSamples = (kSampleRate * 60) / 1000;
	// Drop the oldest decoded audio once a source backs up beyond this, so a stalled
	// consumer can never grow the buffer without bound.
	constexpr size_t kMaxBufferedSamples = kBlockFrames * 8;
	// Reap a source that has received no frame for this long, as a safety net for a
	// missed track-close notification.
	constexpr uint64_t kIdleTimeoutMs = 3000;

	uint64_t NowMs()
	{
		using namespace std::chrono;
		return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}
}

SpatialAudioMixer::SourceChannel::~SourceChannel()
{
	if (decoder)
	{
		opus_decoder_destroy(decoder);
		decoder = nullptr;
	}
}

SpatialAudioMixer::SpatialAudioMixer()
{
}

SpatialAudioMixer::~SpatialAudioMixer()
{
	Stop();
}

void SpatialAudioMixer::Start(AudioPlayer *player)
{
	if (running.load())
		return;
	audioPlayer = player;
	running.store(true);
	outputThread = std::thread(&SpatialAudioMixer::OutputThread, this);
}

void SpatialAudioMixer::Stop()
{
	if (running.exchange(false))
	{
		if (outputThread.joinable())
			outputThread.join();
	}
	std::lock_guard<std::mutex> lock(sourcesMutex);
	sources.clear();
	audioPlayer = nullptr;
}

void SpatialAudioMixer::PushOpusFrame(uint64_t nodeUid, const uint8_t *data, size_t size)
{
	if (!data || size == 0)
		return;

	std::shared_ptr<SourceChannel> ch;
	{
		std::lock_guard<std::mutex> lock(sourcesMutex);
		auto it = sources.find(nodeUid);
		if (it == sources.end())
		{
			ch = std::make_shared<SourceChannel>();
			int opusErr = 0;
			ch->decoder = opus_decoder_create(kSampleRate, 1, &opusErr);
			if (!ch->decoder || opusErr != OPUS_OK)
				return;	// cannot decode this source; drop the frame
			sources[nodeUid] = ch;
		}
		else
		{
			ch = it->second;
		}
	}

	ch->lastActiveMs.store(NowMs());
	{
		std::lock_guard<std::mutex> lock(ch->frameMutex);
		ch->frameQueue.push(std::vector<uint8_t>(data, data + size));
	}
}

void SpatialAudioMixer::SetSourceSpatial(uint64_t nodeUid, float leftGain, float rightGain)
{
	std::lock_guard<std::mutex> lock(sourcesMutex);
	auto it = sources.find(nodeUid);
	if (it == sources.end())
		return;
	it->second->leftGain.store(leftGain);
	it->second->rightGain.store(rightGain);
}

void SpatialAudioMixer::RemoveSource(uint64_t nodeUid)
{
	std::lock_guard<std::mutex> lock(sourcesMutex);
	sources.erase(nodeUid);
}

std::vector<uint64_t> SpatialAudioMixer::GetActiveSources() const
{
	std::vector<uint64_t> uids;
	std::lock_guard<std::mutex> lock(sourcesMutex);
	uids.reserve(sources.size());
	for (const auto &kv : sources)
		uids.push_back(kv.first);
	return uids;
}

std::vector<std::shared_ptr<SpatialAudioMixer::SourceChannel>> SpatialAudioMixer::SnapshotChannels() const
{
	std::vector<std::shared_ptr<SourceChannel>> chans;
	std::lock_guard<std::mutex> lock(sourcesMutex);
	chans.reserve(sources.size());
	for (const auto &kv : sources)
		chans.push_back(kv.second);
	return chans;
}

void SpatialAudioMixer::DecodeInto(SourceChannel &ch)
{
	std::vector<std::vector<uint8_t>> frames;
	{
		std::lock_guard<std::mutex> lock(ch.frameMutex);
		while (!ch.frameQueue.empty())
		{
			frames.push_back(std::move(ch.frameQueue.front()));
			ch.frameQueue.pop();
		}
	}
	if (!ch.decoder)
		return;

	int16_t scratch[kMaxDecodeSamples];
	for (auto &f : frames)
	{
		int ns = opus_decode(ch.decoder, f.data(), (opus_int32)f.size(), scratch, kMaxDecodeSamples, 0);
		if (ns > 0)
			ch.pcm.insert(ch.pcm.end(), scratch, scratch + ns);
	}

	// Bound the backlog: if the source has run ahead, drop the oldest samples.
	while (ch.pcm.size() > kMaxBufferedSamples)
		ch.pcm.pop_front();
}

void SpatialAudioMixer::MixBlock(int16_t *out, int blockFrames)
{
	std::vector<float> accum((size_t)blockFrames * 2, 0.0f);
	auto chans = SnapshotChannels();

	for (auto &ch : chans)
	{
		DecodeInto(*ch);
		const float lg = ch->leftGain.load();
		const float rg = ch->rightGain.load();
		const int n = (int)std::min((size_t)blockFrames, ch->pcm.size());
		for (int i = 0; i < n; i++)
		{
			const float s = (float)ch->pcm[i];
			accum[(size_t)i * 2] += s * lg;
			accum[(size_t)i * 2 + 1] += s * rg;
		}
		ch->pcm.erase(ch->pcm.begin(), ch->pcm.begin() + n);
	}

	for (size_t i = 0; i < accum.size(); i++)
	{
		float v = accum[i];
		if (v > 32767.0f) v = 32767.0f;
		if (v < -32768.0f) v = -32768.0f;
		out[i] = (int16_t)v;
	}
}

void SpatialAudioMixer::ReapIdle(uint64_t nowMs)
{
	std::vector<uint64_t> stale;
	{
		std::lock_guard<std::mutex> lock(sourcesMutex);
		for (const auto &kv : sources)
		{
			if (nowMs - kv.second->lastActiveMs.load() > kIdleTimeoutMs && kv.second->pcm.empty())
				stale.push_back(kv.first);
		}
		for (uint64_t uid : stale)
			sources.erase(uid);
	}
}

void SpatialAudioMixer::OutputThread()
{
	std::vector<int16_t> out((size_t)kBlockFrames * 2, 0);
	while (running.load())
	{
		std::memset(out.data(), 0, out.size() * sizeof(int16_t));
		MixBlock(out.data(), kBlockFrames);
		if (audioPlayer)
			audioPlayer->playStream((const uint8_t *)out.data(), out.size() * sizeof(int16_t));
		ReapIdle(NowMs());
	}
}
