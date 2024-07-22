// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

namespace avs
{

/*!
 * Payload decode frequency.
 *
 * Defines at what granularity video payload is being passed to the underlying hardware decoder.
 * Different decoders may have different requirements as to the format of input they accept.
 */
enum class DecodeFrequency
{
	AccessUnit, /*!< Decode every complete access unit (at VCL boundaries). */
	NALUnit,    /*!< Decode every NAL unit. */
};

/*!
 * Video decoder status.
 *
 * Specifies the status of the video decoder at certain step in the video decoding pipeline.
 */
enum class DecoderStatus : uint32_t
{
	DecoderUnavailable					= 0x00000000, /*!< Decoder has not been set up. */
	DecoderAvailable					= 0x0000000F, /*!< Decoder is set up and ready to receive data. */
	ReceivingVideoStream				= 0x000000F0, /*!< Decoder is receiving video data from the server. */
	QueuingVideoStreamBuffer			= 0x00000F00, /*!< Decoder is storing the single received buffer for accumulation and processing. */
	AccumulatingVideoStreamBuffers		= 0x0000F000, /*!< Decoder is collecting multiple single buffers to assemble enough data for a frame. */
	PassingVideoStreamToDecoder			= 0x000F0000, /*!< Decoder is passing the completed frame data for decoding. */
	DecodingVideoStream					= 0x00F00000, /*!< Hardware or software accelerated decoding of the video stream data. */
	ProcessingOutputFrameFromDecoder	= 0x0F000000, /*!< Decoder is processing the output frame for use in graphics APIs. */
	FrameAvailable						= 0xF0000000, /*!< Decoded video frame is available for use in graphics APIs. */
};

/*!
 * Video decoder status names.
 *
 * For use with magic_enum
 */
enum class DecoderStatusNames
{
	DecoderUnavailable,
	DecoderAvailable,
	ReceivingVideoStream,
	QueuingVideoStreamBuffer,
	AccumulatingVideoStreamBuffers,
	PassingVideoStreamToDecoder,
	DecodingVideoStream,
	ProcessingOutputFrameFromDecoder,
	FrameAvailable
};

/*!
 * Video decoder parameters.
 */
struct DecoderParams
{
	/*! Video codec. */
	VideoCodec codec = VideoCodec::Invalid;
	/*! Payload decode frequency. */
	DecodeFrequency decodeFrequency = DecodeFrequency::NALUnit;
	/*! If true prepend AVC Annex B start codes to every NAL unit before decoding. */
	bool prependStartCodes = true;
	/*! If true display is delayed until next Decoder::process() call; this improves pipelining at the expense of additional latency. */
	bool deferDisplay = false;
	/*! If true, the decoder will do 10-bit decoding */
	bool use10BitDecoding = false;
	/*! If using a YUV output format, the decoder will use chroma format YUV444 if true and YUV420 if false.*/
	bool useYUV444ChromaFormat = false;
	/*! True if the encoder uses alpha layer encoding; a separate decoder will be created for alpha. */
	bool useAlphaLayerDecoding = false;
};

/*!
 * Common decoder backend interface.
 *
 * Decoder backend is responsible for decoding compressed video data using a particular hardware decoder.
 * Decoded video frames are outputted to a registered surface.
 */
class DecoderBackendInterface : public UseInternalAllocator
{
public:
	virtual ~DecoderBackendInterface() = default;

	/*!
	 * Initialize hardware decoder.
	 * Decoder backend must be succcesfully initialized before it's ready to accept input.
	 * \param device Graphics API device handle (DirectX or OpenGL).
	 * \param frameWidth Expected video frame width in pixels.
	 * \param frameHeight Expected video frame height in pixels.
	 * \param params Additional decoder configuration settings.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_InvalidDevice if passed device handle is invalid or otherwise unsuitable for this particular decoder.
	 *  - Result::DecoderBackend_InvalidParam if one or more parameters were invalid.
	 *  - Result::DecoderBackend_CodecNotSupported if the selected video codec is not supported by this decoder.
	 *  - Result::DecoderBackend_InitFailed on general initialization failure.
	 */
	virtual Result initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const DecoderParams& params) = 0;

	/*!
	 * Reconfigure hardware decoder with new initialization settings.
	 * Decoder backend must be succcesfully initialized before this function can be called.
	 * \param frameWidth Expected video frame width in pixels.
	 * \param frameHeight Expected video frame height in pixels.
	 * \param params Additional decoder configuration settings.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_NotInitialized if decoder backend has not yet been initialized.
	 *  - Result::DecoderBackend_ReconfigFailed on general initialization failure.
	 */
	virtual Result reconfigure(int frameWidth, int frameHeight, const DecoderParams& params) = 0;

	/*!
	 * Shutdown hardware decoder and release all associated resources.
	 * After decoder backend is shutdown it must be initialized again to accept input.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_ShutdownFailed on general failure.
	 */
	virtual Result shutdown() = 0;

	/*!
	 * Register surface as destination for decoded video frames.
	 * The surface must be compatible with the graphics API device handle passed to initialize().
	 * Decoder backend does not take ownership of the registered surface.
	 * \param surface Surface backend to register as output.
	 * \param alphaSurface Separate surface backend to register as output for alpha only.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_NotInitialized if decoder backend has not yet been initialized.
	 *  - Result::DeocderBackend_SurfaceAlreadyRegistered if surface backend has already been registered with this decoder backend.
	 *  - Result::DecoderBackend_InvalidSurface if passed surface backend is invalid or otherwise unsuitable for this decoder.
	 */
	virtual Result registerSurface(const SurfaceBackendInterface* surface, const SurfaceBackendInterface* alphaSurface = nullptr) = 0;

	/*!
	 * Unregister surface as destination for decoded video frames.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_NotInitialized if decoder backend has not yet been initialized.
	 *  - Result::DecoderBackend_SurfaceNotRegistered if no surface backend has been registered with this decoder backend.
	 *  - Result::DecoderBackend_InvalidSurface if passed surface backend is invalid.
	 */
	virtual Result unregisterSurface() = 0;

	/*!
	 * Decode compressed video payload.
	 * \param buffer Pointer to the beginning of compressed color video data.
	 * \param bufferSizeInBytes Size in bytes of compressed color video data.
	 * \param alphaBuffer Pointer to the beginning of compressed alpha video data.
	 * \param alphaBufferSizeInBytes Size in bytes of compressed alpha video data.
	 * \param payloadType Video payload type in buffer.
	 * \param lastPayload Signifies whether the buffer contains the last segment of the video frame.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_ReadyToDisplay on success and decoder is ready to display output.
	 *  - Result::DecoderBackend_NotInitialized if decoder backend has not yet been initialized.
	 *  - Result::DecoderBackend_InvalidParam if either buffer is nullptr or bufferSizeInBytes is zero.
	 *  - Result::DecoderBackend_InvalidPayload if the specified payloadType is not suitable for this decoder.
	 *  - Result::DecoderBackend_ParseFailed if decoder failed to parse video data.
	 */
	virtual Result decode(const void* buffer, size_t bufferSizeInBytes, const void* alphaBuffer, size_t alphaBufferSizeInBytes, VideoPayloadType payloadType, bool lastPayload) = 0;

	/*!
	 * Display decoded frame on destination surface.
	 * 
	 * \param showAlphaAsColor Determines whether to render the alpha channel to the output surface as color for debugging purposes. 
	 * \return
	 *  - Result::OK on succes.
	 *  - Result::DecoderBackend_DisplayFailed on failure, or if decoder was not yet ready to display.
	 */
	virtual Result display(bool showAlphaAsColor = false) = 0;
};

} // avs