// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <functional>

namespace avs
{
	/*!
	 * Opus Audio Encoder node `[input-active, output-active, 0/0]`
	 *
	 * Encodes 48 kHz int16 mono PCM into Opus packets (20 ms frames,
	 * payload type 111). PCM is submitted via `submitPcmFrame` from the
	 * audio-capture thread; `process()` drains the queue, encodes each
	 * 20 ms frame, and invokes the registered encoded-frame callback
	 * (typically wired to `WebRtcNetworkSource::sendOpusFrame`).
	 *
	 * Thread-safe for PCM input on one thread and processing on another.
	 */
	class AVSTREAM_API OpusAudioEncoder final : public PipelineNode
	{
		AVSTREAM_PUBLICINTERFACE(OpusAudioEncoder)
	public:
		/*!
		 * Callback invoked with each encoded Opus packet (20 ms ptime).
		 * The buffer is valid only for the duration of the callback.
		 */
		using EncodedFrameCallback = std::function<void(const uint8_t* data, size_t size)>;

		explicit OpusAudioEncoder();
		~OpusAudioEncoder();

		/*!
		 * Configure encoder.
		 * \param channels - number of channels (1 for mono, 2 for stereo).
		 * \param sampleRate - sample rate in Hz (typically 48000).
		 * \param bitrate - target bitrate in bits/sec (e.g. 24000).
		 *                  0 selects the libopus default.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if already configured.
		 *  - Result::Node_InvalidConfiguration on bad arguments.
		 *  - Result::Failed if `opus_encoder_create` fails.
		 */
		Result configure(int channels, int sampleRate, int bitrate = 24000);

		Result deconfigure() override;

		/*!
		 * Submit raw PCM samples for encoding. Thread-safe.
		 * \param pcm - pointer to int16 interleaved PCM samples
		 * \param sampleCountPerChannel - number of samples per channel in this buffer
		 */
		Result submitPcmFrame(const int16_t* pcm, size_t sampleCountPerChannel);

		/*!
		 * Convenience overload for raw byte buffers (assumes int16 interleaved).
		 */
		Result submitPcmFrame(const uint8_t* pcmBytes, size_t byteSize);

		/*!
		 * Register the callback invoked with each encoded Opus packet.
		 * Pass nullptr to clear. Thread-safe.
		 */
		void setEncodedFrameCallback(EncodedFrameCallback cb);

		/*!
		 * Encode queued PCM in 20 ms frames; invoke the encoded-frame
		 * callback for each output packet.
		 * \sa PipelineNode::process()
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		const char* getDisplayName() const override { return "Opus Audio Encoder"; }
	};

} // avs
