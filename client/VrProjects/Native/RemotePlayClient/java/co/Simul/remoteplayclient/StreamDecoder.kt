// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient

import android.media.MediaCodec
import android.media.MediaFormat
import android.util.Log
import android.view.Surface
import java.io.InputStream
import java.util.*

import co.Simul.remoteplayclient.parsers.PacketParser
import co.Simul.remoteplayclient.parsers.PacketParserH264
import co.Simul.remoteplayclient.parsers.PacketParserH265
import co.Simul.remoteplayclient.sources.NetworkStreamSource
import co.Simul.remoteplayclient.sources.StreamSource

enum class VideoCodec {
    H264,
    H265
}

class StreamDecoder(private val mCodec : VideoCodec) : StreamSource.Listener {
    private val mDecoder = MediaCodec.createDecoderByType(mimeType)
    private lateinit var mSource: StreamSource

    private val mimeType get() = when(mCodec) {
        VideoCodec.H264 -> "video/avc"
        VideoCodec.H265 -> "video/hevc"
    }
    private val mParser = when(mCodec) {
        VideoCodec.H264 -> PacketParserH264()
        VideoCodec.H265 -> PacketParserH265()
    }

    fun configure(width: Int, height: Int, surface: Surface) {
        val fmt = MediaFormat.createVideoFormat(mimeType, width, height)
        fmt.setInteger(MediaFormat.KEY_MAX_WIDTH, width)
        fmt.setInteger(MediaFormat.KEY_MAX_HEIGHT, height)
        mDecoder.configure(fmt, surface, null, 0)
        mDecoder.start()

        mSource = NetworkStreamSource(this, 1666)
    }

    fun process() {
        mSource.process()
    }

    private class DecoderState(private val mCodec: VideoCodec) {
        var hasVPS = false
        var hasPPS = false
        var hasSPS = false

        val isConfigured get() = when(mCodec) {
            VideoCodec.H264 -> hasPPS && hasSPS
            VideoCodec.H265 -> hasVPS && hasPPS && hasSPS
        }
        fun updateState(packetType: PacketParser.PacketType) {
            when(packetType) {
                PacketParser.PacketType.VPS -> hasVPS = true
                PacketParser.PacketType.PPS -> hasPPS = true
                PacketParser.PacketType.SPS -> hasSPS = true
            }
        }
    }
    private val mDecoderState = DecoderState(mCodec)
    private val mFrameQueue = LinkedList<Pair<PacketParser.PacketType, ByteArray>>()

    override fun onPacketAvailable(bytes: ByteArray): Boolean {
        var completedFrame = false

        val type = mParser.classify(bytes)
        if (type == PacketParser.PacketType.VCL) {
            if (!mDecoderState.isConfigured) {
                return true
            }
            if(mParser.isFirstVCL(bytes)) {
                // Decode previous frame
                decodeFrame()
                completedFrame = true
            }
        }
        mDecoderState.updateState(type)
        mFrameQueue.add(Pair(type, bytes))
        return !completedFrame
    }

    private fun decodeFrame() {
        while(mFrameQueue.isNotEmpty()) {
            val (packetType, packetBytes) = mFrameQueue.remove()
            val inputBufferFlags = when(packetType) {
                PacketParser.PacketType.VPS -> MediaCodec.BUFFER_FLAG_CODEC_CONFIG
                PacketParser.PacketType.PPS -> MediaCodec.BUFFER_FLAG_CODEC_CONFIG
                PacketParser.PacketType.SPS -> MediaCodec.BUFFER_FLAG_CODEC_CONFIG
                else -> 0
            }

            val inputBufferID = mDecoder.dequeueInputBuffer(0)
            if(inputBufferID >= 0) {
                val inputBuffer = mDecoder.getInputBuffer(inputBufferID)
                val codecBuffer = when(inputBufferFlags) {
                    MediaCodec.BUFFER_FLAG_CODEC_CONFIG -> byteArrayOf(0, 0, 0, 1).plus(packetBytes)
                    else -> byteArrayOf(0, 0, 1).plus(packetBytes)
                }

                inputBuffer.clear()
                inputBuffer.put(codecBuffer)
                mDecoder.queueInputBuffer(inputBufferID, 0, codecBuffer.size, 0, inputBufferFlags)
            }
            else {
                Log.w("StreamDecoder", "Could not dequeue decoder input buffer")
            }
        }

        var bufferInfo = MediaCodec.BufferInfo()
        val outputBufferID = mDecoder.dequeueOutputBuffer(bufferInfo, 0)
        if(outputBufferID >= 0) {
            try {
                mDecoder.releaseOutputBuffer(outputBufferID, true)
            } catch(e: MediaCodec.CodecException) {
                Log.e("StreamDecoder", "Could not release decoder output buffer: ${e.diagnosticInfo}")
            }
        }
    }
}
