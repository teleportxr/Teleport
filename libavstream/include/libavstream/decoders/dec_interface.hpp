// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

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
	 * \param params Additional decoder parameters.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_InvalidDevice if passed device handle is invalid or otherwise unsuitable for this particular decoder.
	 *  - Result::DecoderBackend_InvalidParam if one or more parameters were invalid.
	 *  - Result::DecoderBackend_CodecNotSupported if the selected video codec is not supported by this decoder.
	 *  - Result::DecoderBackend_InitFailed on general initialization failure.
	 */
	virtual Result initialize(const DeviceHandle& device, int frameWidth, int frameHeight, const DecoderParams& params) = 0;

	/*!
	 * Reconfigure hardware decoder.
	 * Decoder backend must be succcesfully initialized before this function can be called.
	 * \param frameWidth Expected video frame width in pixels.
	 * \param frameHeight Expected video frame height in pixels.
	 * \param params Additional decoder parameters.
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
	 * \note Decoder backend does not take ownership of the registered surface.
	 * \param surface Surface backend to register as output.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_NotInitialized if decoder backend has not yet been initialized.
	 *  - Result::DeocderBackend_SurfaceAlreadyRegistered if surface backend has already been registered with this decoder backend.
	 *  - Result::DecoderBackend_InvalidSurface if passed surface backend is invalid or otherwise unsuitable for this decoder.
	 */
	virtual Result registerSurface(const SurfaceBackendInterface* surface) = 0;

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
	 * \param buffer Pointer to the beginning of compressed video data.
	 * \param bufferSizeInBytes Size in bytes of compressed video data.
	 * \param payloadType Video payload type in buffer.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::DecoderBackend_ReadyToDisplay on success and decoder is ready to display output.
	 *  - Result::DecoderBackend_NotInitialized if decoder backend has not yet been initialized.
	 *  - Result::DecoderBackend_InvalidParam if either buffer is nullptr or bufferSizeInBytes is zero.
	 *  - Result::DecoderBackend_InvalidPayload if the specified payloadType is not suitable for this decoder.
	 *  - Result::DecoderBackend_ParseFailed if decoder failed to parse video data.
	 */
	virtual Result decode(const void* buffer, size_t bufferSizeInBytes, VideoPayloadType payloadType, bool lastPayload) = 0;

	/*!
	 * Display decoded frame on destination surface.
	 * \return
	 *  - Result::OK on succes.
	 *  - Result::DecoderBackend_DisplayFailed on failure, or if decoder was not yet ready to display.
	 */
	virtual Result display(bool showAlphaAsColor = false) = 0;
};

} // avs