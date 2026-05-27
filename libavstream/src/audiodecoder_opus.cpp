// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/audiodecoder_opus.h>
#include <opus.h>
#include <queue>
#include <mutex>
#include <cstring>

namespace avs
{
	struct OpusAudioDecoder::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(OpusAudioDecoder, PipelineNode)

		OpusDecoder* opusDecoder = nullptr;
		int channels = 1;
		int sampleRate = 48000;
		bool configured = false;

		// Thread-safe queue for Opus frames pending decode
		std::queue<std::vector<uint8_t>> frameQueue;
		std::mutex queueMutex;

		// Output buffer for decoded PCM (int16, pre-allocated)
		std::vector<int16_t> pcmBuffer;
		size_t maxSamplesPerFrame = 0; // 48kHz * 20ms = 960 samples

		~Private()
		{
			if (opusDecoder)
			{
				opus_decoder_destroy(opusDecoder);
				opusDecoder = nullptr;
			}
		}
	};

	OpusAudioDecoder::OpusAudioDecoder()
		: PipelineNode(new OpusAudioDecoder::Private(this))
	{
		setNumSlots(0, 1);
	}

	OpusAudioDecoder::~OpusAudioDecoder()
	{
		deconfigure();
	}

	Result OpusAudioDecoder::configure(int channels, int sampleRate)
	{
		Private& priv = d();
		if (priv.configured)
			return Result::Node_AlreadyConfigured;

		if (channels <= 0 || sampleRate <= 0)
			return Result::Node_InvalidConfiguration;

		// Create Opus decoder
		int opusErr = 0;
		priv.opusDecoder = opus_decoder_create(sampleRate, channels, &opusErr);
		if (!priv.opusDecoder || opusErr != OPUS_OK)
		{
			AVSLOG(Error) << "OpusAudioDecoder: Failed to create opus decoder: " << opus_strerror(opusErr);
			return Result::Failed;
		}

		priv.channels = channels;
		priv.sampleRate = sampleRate;
		// Max frame size: 48kHz * 60ms (Opus max) = 2880 samples per channel
		priv.maxSamplesPerFrame = (sampleRate * 60) / 1000;
		priv.pcmBuffer.resize(priv.maxSamplesPerFrame * channels);
		priv.configured = true;

		name = "Opus Audio Decoder";
		return Result::OK;
	}

	Result OpusAudioDecoder::deconfigure()
	{
		Private& priv = d();
		if (!priv.configured)
			return Result::Node_NotConfigured;

		if (priv.opusDecoder)
		{
			opus_decoder_destroy(priv.opusDecoder);
			priv.opusDecoder = nullptr;
		}
		priv.configured = false;
		priv.pcmBuffer.clear();

		return unlinkOutput();
	}

	Result OpusAudioDecoder::submitOpusFrame(const uint8_t* opusData, size_t opusDataSize)
	{
		Private& priv = d();
		if (!priv.configured)
			return Result::Node_NotConfigured;

		std::lock_guard<std::mutex> lock(priv.queueMutex);
		priv.frameQueue.push(std::vector<uint8_t>(opusData, opusData + opusDataSize));
		return Result::OK;
	}

	Result OpusAudioDecoder::process(uint64_t timestamp, uint64_t deltaTime)
	{
		Private& priv = d();
		if (!priv.configured)
			return Result::Node_NotConfigured;

		AudioTargetInterface* target = dynamic_cast<AudioTargetInterface*>(getOutput(0));
		if (!target)
			return Result::Node_InvalidOutput;

		// Decode all queued frames
		std::lock_guard<std::mutex> lock(priv.queueMutex);
		while (!priv.frameQueue.empty())
		{
			auto& opusFrame = priv.frameQueue.front();
			int nsamples = opus_decode(priv.opusDecoder,
				opusFrame.data(), static_cast<opus_int32>(opusFrame.size()),
				priv.pcmBuffer.data(), static_cast<int>(priv.maxSamplesPerFrame), 0);

			if (nsamples < 0)
			{
				AVSLOG(Warning) << "OpusAudioDecoder: opus_decode error: " << opus_strerror(nsamples);
				priv.frameQueue.pop();
				continue;
			}

			// Convert int16 samples to bytes and push to audio target
			size_t pcmByteSize = nsamples * priv.channels * sizeof(int16_t);
			auto result = target->getAudioTargetBackendInterface()->process(
				priv.pcmBuffer.data(), pcmByteSize, AudioPayloadType::Capture);

			if (!result)
				AVSLOG(Warning) << "OpusAudioDecoder: Failed to process PCM output";

			priv.frameQueue.pop();
		}

		return Result::OK;
	}

	Result OpusAudioDecoder::onInputLink(int slot, PipelineNode* node)
	{
		return Result::OK;
	}

	Result OpusAudioDecoder::onOutputLink(int slot, PipelineNode* node)
	{
		if (slot != 0)
			return Result::Node_InvalidSlot;
		AudioTargetInterface* target = dynamic_cast<AudioTargetInterface*>(node);
		if (!target)
			return Result::Node_Incompatible;
		return Result::OK;
	}

	void OpusAudioDecoder::onOutputUnlink(int slot, PipelineNode* node)
	{
	}

} // avs
