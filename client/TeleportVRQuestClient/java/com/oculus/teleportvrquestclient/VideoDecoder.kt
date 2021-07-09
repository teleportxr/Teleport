// (C) Copyright 2018 Simul.co

package co.simul.teleportvrquestclient

import android.graphics.SurfaceTexture
import android.media.MediaCodec
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer

enum class VideoCodec {
    H264,
    H265
}

enum class PayloadType {
    FirstVCL,       /*!< Video Coding Layer unit (first VCL in an access unit). */
    VCL,            /*!< Video Coding Layer unit (any subsequent in each access unit). */
    VPS,            /*!< Video Parameter Set (HEVC only) */
    SPS,            /*!< Sequence Parameter Set */
    PPS,            /*!< Picture Parameter Set */
    ALE,            /*!< Custom name. NAL unit with alpha layer encoding metadata (HEVC only). */
    OtherNALUnit,   /*!< Other NAL unit. */
    AccessUnit      /*!< Entire access unit (possibly multiple NAL units). */
}

class VideoDecoder(private val mDecoderProxy: Long, private val mCodecTypeIndex: Int) : SurfaceTexture.OnFrameAvailableListener
{
    private val mDecoder = MediaCodec.createDecoderByType(mCodecMimeType)
    private var mDecoderConfigured = false
    private var mDisplayRequests = 0

    fun initialize(colorSurface: SurfaceTexture, frameWidth: Int, frameHeight: Int)
    {
        if(mDecoderConfigured)
        {
            Log.e("RemotePlay", "VideoDecoder: Cannot initialize: already configured")
            return
        }

        val format = MediaFormat.createVideoFormat(mCodecMimeType, frameWidth, frameHeight)
        format.setInteger(MediaFormat.KEY_MAX_WIDTH, frameWidth)
        format.setInteger(MediaFormat.KEY_MAX_HEIGHT, frameHeight)

        colorSurface.setOnFrameAvailableListener(this)

        mDecoder.configure(format, Surface(colorSurface), null, 0)
        mDecoder.start()

        mDecoderConfigured = true
    }

    fun shutdown()
    {
        if(!mDecoderConfigured)
        {
            Log.e("RemotePlay", "VideoDecoder: Cannot shutdown: not configured")
            return
        }

        mDecoder.flush()
        mDecoder.stop()
        mDecoderConfigured = false
        mDisplayRequests = 0
    }

    fun decode(buffer: ByteBuffer, payloadTypeIndex: Int, lastPayload: Boolean): Boolean
    {
        if(!mDecoderConfigured)
        {
            Log.e("RemotePlay", "VideoDecoder: Cannot decode buffer: not configured")
            return false
        }

        val payloadType = getPayloadTypeFromIndex(payloadTypeIndex)
        var payloadFlags = when(payloadType)
        {
            PayloadType.VPS -> MediaCodec.BUFFER_FLAG_CODEC_CONFIG
            PayloadType.PPS -> MediaCodec.BUFFER_FLAG_CODEC_CONFIG
            PayloadType.SPS -> MediaCodec.BUFFER_FLAG_CODEC_CONFIG
            PayloadType.ALE -> MediaCodec.BUFFER_FLAG_CODEC_CONFIG
            else -> 0
        }
        val startCodes = when(payloadFlags)
        {
            MediaCodec.BUFFER_FLAG_CODEC_CONFIG -> byteArrayOf(0, 0, 0, 1)
            else -> byteArrayOf(0, 0, 1)
        }

        if(!lastPayload)
        {
            // Signifies partial frame data. For all VCLs in a frame besides the last one. Needed for H264.
            if (payloadFlags == 0) {
                payloadFlags = 8;
            }
        }

        val inputBuffer = startCodes.plus(ByteArray(buffer.remaining()))
        buffer.get(inputBuffer, startCodes.size, buffer.remaining())
        val bufferId = queueInputBuffer(inputBuffer, payloadFlags)
        if(lastPayload && bufferId >= 0)
        {
            ++mDisplayRequests
            return true
        }

        return false
    }

    fun display(): Boolean
    {
        if(!mDecoderConfigured)
        {
            Log.e("RemotePlay", "VideoDecoder: Cannot display output: not configured")
            return false
        }
        while(mDisplayRequests > 0)
        {
            if (releaseOutputBuffer(mDisplayRequests == 1) > -2)
                mDisplayRequests--
        }
        return true
    }

    private fun queueInputBuffer(buffer: ByteArray, flags: Int) : Int
    {
        val inputBufferID = mDecoder.dequeueInputBuffer(0) // microseconds
        if(inputBufferID >= 0)
        {
            val inputBuffer = mDecoder.getInputBuffer(inputBufferID)
            inputBuffer?.clear()
            inputBuffer?.put(buffer)
            mDecoder.queueInputBuffer(inputBufferID, 0, buffer.size, 0, flags)
        }
        else
        {
            //Log.w("RemotePlay", "VideoDecoder: Could not dequeue decoder input buffer")
        }
        return inputBufferID
    }

    private fun releaseOutputBuffer(render: Boolean) : Int
    {
        var bufferInfo = MediaCodec.BufferInfo()
        val outputBufferID = mDecoder.dequeueOutputBuffer(bufferInfo, 0)
        if(outputBufferID >= 0)
        {
            mDecoder.releaseOutputBuffer(outputBufferID, render)
        }
        else
        {
            //Log.w("RemotePlay", "VideoDecoder: Could not dequeue decoder output buffer");
        }
        return outputBufferID
    }

    external fun nativeFrameAvailable(decoderProxy: Long)
    override fun onFrameAvailable(surfaceTexture: SurfaceTexture?) {
        nativeFrameAvailable(mDecoderProxy)
    }

    private val mCodecType get() = when(mCodecTypeIndex) {
        1 -> VideoCodec.H264
        2 -> VideoCodec.H265
        else -> throw IllegalArgumentException("Invalid codec type index")
    }
    private val mCodecMimeType get() = when(mCodecType) {
        VideoCodec.H264 -> "video/avc"
        VideoCodec.H265 -> "video/hevc"
    }
    private fun getPayloadTypeFromIndex(payloadTypeIndex: Int) = when(payloadTypeIndex) {
        0 -> PayloadType.FirstVCL
        1 -> PayloadType.VCL
        2 -> PayloadType.VPS
        3 -> PayloadType.SPS
        4 -> PayloadType.PPS
        5 -> PayloadType.ALE
        6 -> PayloadType.OtherNALUnit
        7 -> PayloadType.AccessUnit
        else -> PayloadType.OtherNALUnit
    }
}