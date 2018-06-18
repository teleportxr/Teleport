// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient.sources

import java.io.EOFException
import java.io.DataInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

class LocalStreamSource(private val mListener: StreamSource.Listener,
                        private val mInputStream: DataInputStream) : StreamSource {

    override fun process(): StreamSource.Result {
        try {
            while(processPacket()) {}
        } catch(e: EOFException) {
            return StreamSource.Result.END_OF_STREAM
        }
        return StreamSource.Result.CONTINUE
    }

    override fun close() {
        // Empty.
    }

    private fun processPacket(): Boolean {
        val header = ByteArray(4)
        mInputStream.readFully(header)
        val packetSize = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN).int

        val packet = ByteArray(packetSize)
        mInputStream.readFully(packet)
        return mListener.onPacketAvailable(packet)
    }
}
