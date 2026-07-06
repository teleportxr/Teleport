// Unit tests for the client-side spatial audio mixer (TeleportAudio/SpatialAudioMixer).
// Covers the pure spatialisation maths (ComputeStereoGains) and multi-source mixing
// (MixBlock), the latter exercising the SFU/multi-voice path without a live server.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include <opus.h>

#include "TeleportAudio/SpatialAudioMixer.h"

using teleport::audio::AudioVec3;
using teleport::audio::ComputeStereoGains;
using teleport::audio::SpatialAudioMixer;
using teleport::audio::StereoGains;
using Catch::Approx;

TEST_CASE("ComputeStereoGains pans by azimuth", "[audio][spatial]")
{
	const AudioVec3 listener{0, 0, 0};
	const AudioVec3 right{1, 0, 0};	// listener faces -Z (say), right is +X

	SECTION("source hard left is left-heavy")
	{
		StereoGains g = ComputeStereoGains(listener, right, {-10, 0, 0});
		REQUIRE(g.left > g.right);
	}
	SECTION("source hard right is right-heavy")
	{
		StereoGains g = ComputeStereoGains(listener, right, {10, 0, 0});
		REQUIRE(g.right > g.left);
	}
	SECTION("source dead ahead is centred (equal power)")
	{
		StereoGains g = ComputeStereoGains(listener, right, {0, 0, 10});
		REQUIRE(g.left == Approx(g.right));
	}
}

TEST_CASE("ComputeStereoGains attenuates with distance", "[audio][spatial]")
{
	const AudioVec3 listener{0, 0, 0};
	const AudioVec3 right{1, 0, 0};

	SECTION("beyond maxDistance is silent")
	{
		StereoGains g = ComputeStereoGains(listener, right, {0, 0, 200}, 1.0f, 100.0f);
		REQUIRE(g.left == Approx(0.0f));
		REQUIRE(g.right == Approx(0.0f));
	}
	SECTION("hard right at refDistance is unity on the right channel")
	{
		StereoGains g = ComputeStereoGains(listener, right, {1, 0, 0}, 1.0f, 100.0f);
		REQUIRE(g.right == Approx(1.0f).margin(1e-4));
		REQUIRE(g.left == Approx(0.0f).margin(1e-4));
	}
	SECTION("nearer is louder than farther")
	{
		StereoGains near = ComputeStereoGains(listener, right, {0, 0, 5});
		StereoGains far = ComputeStereoGains(listener, right, {0, 0, 50});
		REQUIRE(near.left > far.left);
	}
}

namespace
{
	// One 20 ms mono Opus frame (960 samples @ 48 kHz) of a loud 440 Hz tone.
	std::vector<uint8_t> EncodeToneFrame()
	{
		int err = 0;
		OpusEncoder *enc = opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &err);
		REQUIRE(enc != nullptr);
		REQUIRE(err == OPUS_OK);

		std::vector<int16_t> pcm(960);
		for (int i = 0; i < 960; i++)
			pcm[i] = (int16_t)(12000.0 * std::sin(2.0 * 3.14159265358979 * 440.0 * i / 48000.0));

		std::vector<uint8_t> out(4000);
		int n = opus_encode(enc, pcm.data(), 960, out.data(), (opus_int32)out.size());
		opus_encoder_destroy(enc);
		REQUIRE(n > 0);
		out.resize((size_t)n);
		return out;
	}

	double ChannelEnergy(const std::vector<int16_t> &interleaved, int channel)
	{
		double e = 0;
		for (size_t i = 0; i < interleaved.size() / 2; i++)
			e += std::abs((double)interleaved[i * 2 + channel]);
		return e;
	}
}

TEST_CASE("SpatialAudioMixer mixes two panned sources", "[audio][spatial][mixer]")
{
	const std::vector<uint8_t> frame = EncodeToneFrame();

	SpatialAudioMixer mixer;	// no Start(): drive MixBlock synchronously
	const uint64_t uidA = 1001, uidB = 1002;

	mixer.PushOpusFrame(uidA, frame.data(), frame.size());
	mixer.PushOpusFrame(uidB, frame.data(), frame.size());
	mixer.SetSourceSpatial(uidA, 1.0f, 0.0f);	// A hard left
	mixer.SetSourceSpatial(uidB, 0.0f, 1.0f);	// B hard right

	REQUIRE(mixer.GetActiveSources().size() == 2);

	std::vector<int16_t> out((size_t)960 * 2, 0);
	mixer.MixBlock(out.data(), 960);

	const double left = ChannelEnergy(out, 0);
	const double right = ChannelEnergy(out, 1);
	// Both sources contributed, each isolated to its panned channel.
	REQUIRE(left > 0.0);
	REQUIRE(right > 0.0);
}

TEST_CASE("SpatialAudioMixer isolates a hard-left source to the left channel", "[audio][spatial][mixer]")
{
	const std::vector<uint8_t> frame = EncodeToneFrame();
	SpatialAudioMixer mixer;

	mixer.PushOpusFrame(42, frame.data(), frame.size());
	mixer.SetSourceSpatial(42, 1.0f, 0.0f);

	std::vector<int16_t> out((size_t)960 * 2, 0);
	mixer.MixBlock(out.data(), 960);

	REQUIRE(ChannelEnergy(out, 0) > 0.0);
	REQUIRE(ChannelEnergy(out, 1) == Approx(0.0));
}

TEST_CASE("SpatialAudioMixer drops a removed source", "[audio][spatial][mixer]")
{
	const std::vector<uint8_t> frame = EncodeToneFrame();
	SpatialAudioMixer mixer;

	mixer.PushOpusFrame(7, frame.data(), frame.size());
	REQUIRE(mixer.GetActiveSources().size() == 1);
	mixer.RemoveSource(7);
	REQUIRE(mixer.GetActiveSources().empty());

	std::vector<int16_t> out((size_t)960 * 2, 0);
	mixer.MixBlock(out.data(), 960);	// no sources -> silence, must not crash
	REQUIRE(ChannelEnergy(out, 0) == Approx(0.0));
	REQUIRE(ChannelEnergy(out, 1) == Approx(0.0));
}
