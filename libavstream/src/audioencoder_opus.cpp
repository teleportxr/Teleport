// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/audioencoder_opus.h>
#include <opus.h>
#include <deque>
#include <mutex>
#include <cstring>

namespace avs
{
	struct OpusAudioEncoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(OpusAudioEncoder, PipelineNode)

		::OpusEncoder* opusEncoder = nullptr;
		int channels = 1;
		int sampleRate = 48000;
		int bitrate = 24000;
		bool configured = false;
		// 20 ms ptime at 48 kHz mono = 960 samples per channel.
		size_t samplesPerFrame = 960;

		// Pending PCM samples (interleaved int16). Accumulates partial
		// buffers until a full 20 ms frame is available.
		std::deque<int16_t> pcmBacklog;
		std::mutex pcmMutex;

		std::vector<int16_t> frameScratch; // samplesPerFrame * channels
		std::vector<uint8_t> opusOut;	   // up to ~4000 bytes per Opus packet

		OpusAudioEncoder::EncodedFrameCallback callback;
		std::mutex callbackMutex;

		~Private()
		{
			if (opusEncoder)
			{
				opus_encoder_destroy(opusEncoder);
				opusEncoder = nullptr;
			}
		}
	};

	OpusAudioEncoder::OpusAudioEncoder()
		: PipelineNode(new OpusAudioEncoder::Private(this))
	{
		setNumSlots(0, 0);
	}

	OpusAudioEncoder::~OpusAudioEncoder()
	{
		deconfigure();
	}

	Result OpusAudioEncoder::configure(int channels, int sampleRate, int bitrate)
	{
		Private& priv = d();
		if (priv.configured)
			return Result::Node_AlreadyConfigured;
		if (channels <= 0 || sampleRate <= 0)
			return Result::Node_InvalidConfiguration;

		int opusErr = 0;
		priv.opusEncoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_VOIP, &opusErr);
		if (!priv.opusEncoder || opusErr != OPUS_OK)
		{
			AVSLOG(Error) << "OpusAudioEncoder: opus_encoder_create failed: " << opus_strerror(opusErr);
			return Result::Failed;
		}

		if (bitrate > 0)
			opus_encoder_ctl(priv.opusEncoder, OPUS_SET_BITRATE(bitrate));
		opus_encoder_ctl(priv.opusEncoder, OPUS_SET_INBAND_FEC(1));
		opus_encoder_ctl(priv.opusEncoder, OPUS_SET_DTX(1));

		priv.channels = channels;
		priv.sampleRate = sampleRate;
		priv.bitrate = bitrate;
		priv.samplesPerFrame = (sampleRate * 20) / 1000; // 20 ms ptime
		priv.frameScratch.assign(priv.samplesPerFrame * channels, 0);
		priv.opusOut.assign(4000, 0);
		priv.configured = true;
		name = "Opus Audio Encoder";
		return Result::OK;
	}

	Result OpusAudioEncoder::deconfigure()
	{
		Private& priv = d();
		if (!priv.configured)
			return Result::Node_NotConfigured;
		if (priv.opusEncoder)
		{
			opus_encoder_destroy(priv.opusEncoder);
			priv.opusEncoder = nullptr;
		}
		{
			std::lock_guard<std::mutex> lock(priv.pcmMutex);
			priv.pcmBacklog.clear();
		}
		priv.configured = false;
		return Result::OK;
	}

	Result OpusAudioEncoder::submitPcmFrame(const int16_t* pcm, size_t sampleCountPerChannel)
	{
		Private& priv = d();
		if (!priv.configured)
			return Result::Node_NotConfigured;
		const size_t total = sampleCountPerChannel * priv.channels;
		std::lock_guard<std::mutex> lock(priv.pcmMutex);
		priv.pcmBacklog.insert(priv.pcmBacklog.end(), pcm, pcm + total);
		return Result::OK;
	}

	Result OpusAudioEncoder::submitPcmFrame(const uint8_t* pcmBytes, size_t byteSize)
	{
		// Assume int16 interleaved PCM (the format produced by LinuxAudioPlayer at 16-bit).
		return submitPcmFrame(reinterpret_cast<const int16_t*>(pcmBytes), byteSize / sizeof(int16_t) / d().channels);
	}

	void OpusAudioEncoder::setEncodedFrameCallback(EncodedFrameCallback cb)
	{
		Private& priv = d();
		std::lock_guard<std::mutex> lock(priv.callbackMutex);
		priv.callback = std::move(cb);
	}

	Result OpusAudioEncoder::process(uint64_t, uint64_t)
	{
		Private& priv = d();
		if (!priv.configured)
			return Result::Node_NotConfigured;

		const size_t samplesPerFrameTotal = priv.samplesPerFrame * priv.channels;
		EncodedFrameCallback cb;
		{
			std::lock_guard<std::mutex> lock(priv.callbackMutex);
			cb = priv.callback;
		}

		while (true)
		{
			{
				std::lock_guard<std::mutex> lock(priv.pcmMutex);
				if (priv.pcmBacklog.size() < samplesPerFrameTotal)
					break;
				for (size_t i = 0; i < samplesPerFrameTotal; ++i)
				{
					priv.frameScratch[i] = priv.pcmBacklog.front();
					priv.pcmBacklog.pop_front();
				}
			}

			int bytes = opus_encode(priv.opusEncoder,
				priv.frameScratch.data(), static_cast<int>(priv.samplesPerFrame),
				priv.opusOut.data(), static_cast<opus_int32>(priv.opusOut.size()));
			if (bytes < 0)
			{
				AVSLOG(Warning) << "OpusAudioEncoder: opus_encode failed: " << opus_strerror(bytes);
				continue;
			}
			if (bytes > 0 && cb)
				cb(priv.opusOut.data(), static_cast<size_t>(bytes));
		}
		return Result::OK;
	}

} // avs
