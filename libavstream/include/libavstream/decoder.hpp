// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/decoders/dec_interface.hpp>
#include <memory>
#include <functional>

namespace avs
{

	/*! Decoder backend type. */
	enum class DecoderBackend
	{
		Any,    /*!< Any backend (auto-detect during configuration). */
		Custom, /*!< Custom external backend. */
		NVIDIA, /*!< NVIDIA CUVID backend. */
	};

	struct DecoderState
	{
		bool hasVPS = false;
		bool hasPPS = false;
		bool hasSPS = false;
		bool hasALE = false;

		bool isReady(VideoCodec codec) const
		{
			switch (codec)
			{
			case VideoCodec::H264:
				return hasPPS && hasSPS;
			case VideoCodec::HEVC:
				return hasVPS && hasPPS && hasSPS;
			default:
				return false;
			}
		}
	};

	/*!
	 * Video decoder node `[input-active, output-active, 1/1]`
	 *
	 * Reads packets of encoded video stream and outputs decoded frames to a surface.
	 * - Compatible inputs: Any node implementing PacketInterface.
	 * - Compatible outputs: Any node implementing SurfaceInterface.
	 */
	class AVSTREAM_API Decoder final : public Node
	{
		AVSTREAM_PUBLICINTERFACE(Decoder)
		Decoder::Private *m_data;
	public:
		/*!
		 * Constructor.
		 * \param backend Decoder backend type to use.
		 */
		explicit Decoder(DecoderBackend backend = DecoderBackend::Any);

		/*!
		 * Constructor.
		 * \note Decoder node takes ownership of the backend instance.
		 * \param backend Custom decoder backend instance.
		 */
		explicit Decoder(DecoderBackendInterface* backend);

		~Decoder();

		/*!
		 * Configure decoder.
		 * \param device Graphics API device handle (DirectX or OpenGL).
		 * \param frameWidth Expected video frame width in pixels.
		 * \param frameHeight Expected video frame height in pixels.
		 * \param params Additional decoder parameters.
		 * \param params callback for extracting extra video data
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if decoder was already in configured state.
		 *  - Result::Decoder_NoSuitableBackendFound if there's no usable decoder backend on the system.
		 *  - Any error result returned by DecoderBackendInterface::initialize().
		 */
		Result configure(const DeviceHandle& device, int frameWidth, int frameHeight, const DecoderParams& params, uint8_t streamId, std::function<void(const uint8_t* data, size_t dataSize)> extraDataCallback);


		/*!
		 * Configure reconfigure.
		 * \param frameWidth Expected video frame width in pixels.
		 * \param frameHeight Expected video frame height in pixels.
		 * \param params Additional decoder parameters.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder is not in configured state.
		 *  - Any error result returned by DecoderBackendInterface::reconfigure().
		 */
		Result reconfigure(int frameWidth, int frameHeight, const DecoderParams& params);

		/*!
		 * Deconfigure decoder and release all associated resources.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Any error result returned by DecoderBackendInterface::shutdown().
		 */
		Result deconfigure() override;

		/*!
		 * Deconfigure decoder and release all associated resources.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Result::Decoder_SurfaceNotRegistered if an input surface is not registered.
		 *  - Any error result returned by DecoderBackendInterface::unregisterSurface().
		 */
		Result unregisterSurface();

		uint8_t getStreamId() const;

		/*!
		 * Process as much encoded video data as available on input and decode zero or more frames.
		 * \sa Node::process()
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_NotConfigured if decoder was not in configured state.
		 *  - Result::Node_InvalidInput if no compatible input node is linked to input slot 0 or most recently read input packet is invalid.
		 *  - Result::Node_InvalidOutput if no compatible output node is linked to output slot 0.
		 *  - Result::IO_Empty if no input data is available for decoding.
		 *  - Any error result returned by PacketInterface::readPacket().
		 *  - Any error result returned by DecoderBackendInterface::decode().
		 */
		Result process(uint64_t timestamp, uint64_t deltaTime) override;

		/*!
		 * Set custom decoder backend.
		 * \note Decoder node takes ownership of the backend instance.
		 * \param backend Custom decoder backend instance.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if decoder was already in configured state.
		 */
		Result setBackend(DecoderBackendInterface* backend);

		/*!
		 * Get node display name (for reporting & profiling).
		 */
		const char* getDisplayName() const override { return "Decoder"; }

		/*!
		 * If a transform has been sent yet.
		 */
		bool hasValidTransform() const;

		/*!
		 * If due to a decoder packet loss, a new IDR frame is needed from the server's encoder to prevent corrupted video.
		 */
		bool idrRequired() const;

		long long getTotalFramesProcessed() const;

		void toggleShowAlphaAsColor() { m_showAlphaAsColor = !m_showAlphaAsColor; }

		int testCount = 0;
	private:
		Result onInputLink(int slot, Node* node) override;
		Result onOutputLink(int slot, Node* node) override;
		void onOutputUnlink(int slot, Node* node) override;
		/*!
		* Register the decoder's surface texture
		* \param Pointer to surface interface
		* \return
		*  - Result::OK on success.
		*  - Result::Decoder_SurfaceAlreadyRegistered if an input surface is already registered.
		*  - Any error result returned by DecoderBackendInterface::registerSurface().
		*/
		Result registerSurface(SurfaceInterface* surface);

	// Moved from Decoder::Private
		std::unique_ptr<class DecoderBackendInterface> m_backend;
		std::unique_ptr<class NALUParserInterface> m_parser;
		DecoderBackend m_selectedBackendType;
		DecoderParams m_params = {};
		DecoderState m_state = {};

		uint64_t m_currentFrameNumber = 0;
		std::vector<uint8_t> m_frameBuffer;
		NetworkFrameInfo m_frame;
		size_t m_extraDataSize = 0;
		size_t m_firstVCLOffset = 0;
		int m_interimFramesProcessed = 0;
		uint8_t m_streamId = 0;
		bool m_idrRequired = false;
		bool m_configured = false;
		bool m_displayPending = false;
		bool m_surfaceRegistered = false;
		bool m_showAlphaAsColor = false;

		std::unique_ptr<class StreamParserInterface> m_vid_parser;
		std::function<void(const uint8_t * data, size_t dataSize)> m_extraDataCallback;

		Result processPayload(const uint8_t* buffer, size_t dataSize, size_t dataOffset, bool isLastPayload);
		Result DisplayFrame();
	};

} // avs