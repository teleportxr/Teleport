// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient.sources

interface StreamSource {
    interface Listener {
        fun onPacketAvailable(bytes : ByteArray): Boolean
    }

    fun process() : Result
    fun close()

    enum class Result {
        CONTINUE,
        END_OF_STREAM,
        ERROR,
    }
}