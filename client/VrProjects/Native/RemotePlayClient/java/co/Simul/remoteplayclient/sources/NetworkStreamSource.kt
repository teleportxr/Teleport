// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient.sources

import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.nio.ByteBuffer
import java.util.*
import java.util.concurrent.Executors
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

typealias Sequence = Int
fun Sequence.next(): Sequence = (this + 1) and 0x7FFF
fun Sequence.prev(): Sequence = (this - 1) and 0x7FFF
fun Sequence.isNextOf(other: Sequence): Boolean = other.next() == this
fun Sequence.isPrevOf(other: Sequence): Boolean = other.prev() == this

data class SequenceRange(var start: Sequence = 0, var end: Sequence = start)

data class NetworkPacket(val timestamp: Int, val last: Boolean, val payload: ByteArray)

class NetworkStreamSource(private val mListener: StreamSource.Listener,
                          port: Int): StreamSource {

    private class NetworkService(port: Int, private val mPacketQueue: LinkedBlockingQueue<ByteArray>): Runnable {
        private val mSocket = DatagramSocket(port)
        private val mBuffer = ByteArray(MTU)
        private val mPacket = DatagramPacket(mBuffer, mBuffer.size)

        override fun run() {
            mSocket.reuseAddress = true
            while(true) {
                mSocket.receive(mPacket)
                mPacketQueue.put(mPacket.data.copyOfRange(mPacket.offset, mPacket.length))
            }
        }

        companion object {
            const val MTU = 1500
        }
    }
    private class NetworkStatTask(private val mIncomingQueue: Queue<ByteArray>,
                                  private val mNetworkPacketMap: Map<Sequence, NetworkPacket>): TimerTask() {
        override fun run() {
            Log.w("STAT", "IncomignQueue: ${mIncomingQueue.size}, NetPacketMap: ${mNetworkPacketMap.size}")
        }
    }

    private val mIncomingQueue = LinkedBlockingQueue<ByteArray>(INCOMING_QUEUE_CAPACITY)
    private val mNetworkPacketMap = TreeMap<Sequence, NetworkPacket>()
    private val mStatTimer = Timer()

    init {
        val mExecutorService = Executors.newSingleThreadExecutor()
        mExecutorService.execute(NetworkService(port, mIncomingQueue))
        mStatTimer.schedule(NetworkStatTask(mIncomingQueue, mNetworkPacketMap), STAT_INTERVAL, STAT_INTERVAL)
    }

    override fun process(): StreamSource.Result {
        receiveNetworkPackets()
        dispatchDecoderPackets()
        return StreamSource.Result.CONTINUE
    }

    private fun receiveNetworkPackets() {
        while(true) {
            val mIncomingPacket: ByteArray? = mIncomingQueue.poll(POLL_TIMEOUT, TimeUnit.MILLISECONDS)
            if(mIncomingPacket == null) {
                break
            }
            else {
                val buffer = ByteBuffer.wrap(mIncomingPacket)

                val streamIndex = buffer.get()
                val sequence    = buffer.getU16LE()
                val timestamp   = buffer.getU16LE()
                val payloadSize = buffer.getU16LE()

                val sequenceID    =  sequence and 0x7FFF
                val lastPacketNAL = (sequence and 0x8000) > 0

                val payload = ByteArray(payloadSize)
                buffer.get(payload)
                mNetworkPacketMap[sequenceID] = NetworkPacket(timestamp, lastPacketNAL, payload)
            }
        }
    }

    private var mLastDispatchedTimestamp = -1

    private fun dispatchDecoderPackets() {
        while(mNetworkPacketMap.isNotEmpty()) {
            val packetRange = findContiguousDecoderPacket()
            if(packetRange != null) {
                var packetBytes = ByteArray(0)
                var packetSequence = packetRange.start
                while(packetSequence != packetRange.end) {
                    mNetworkPacketMap[packetSequence]?.let {
                        packetBytes = packetBytes.plus(it.payload)
                    }
                    mNetworkPacketMap.remove(packetSequence)
                    packetSequence = packetSequence.next()
                }
                mListener.onPacketAvailable(packetBytes)
            }
            else {
                break
            }
        }
    }

    private fun findContiguousDecoderPacket(): SequenceRange? {
        val range = SequenceRange()
        range.start = mNetworkPacketMap.firstKey()
        range.end   = range.start

        // Scan in two passes to handle wrap-around
        for(pass in 1..2) {
            for (entry in mNetworkPacketMap) {
                val sequence = entry.key
                val packet = entry.value

                if (sequence != range.start) {
                    if (sequence.isNextOf(range.end)) {
                        range.end = sequence
                    } else {
                        range.start = sequence
                        range.end = range.start
                    }
                }
                if (packet.last) {
                    // Return half-open range [start,end) for easy iteration.
                    return SequenceRange(range.start, range.end.next())
                }
            }
        }
        return null
    }

    private fun ByteBuffer.getU16LE(): Int {
        val lo = get().toInt() and 0xFF
        val hi = get().toInt() and 0xFF
        return (hi shl 8) or lo
    }

    companion object {
        const val INCOMING_QUEUE_CAPACITY = 256
        const val POLL_TIMEOUT: Long = 1
        const val STAT_INTERVAL: Long = 1000
    }
}