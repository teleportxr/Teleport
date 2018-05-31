// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient.parsers

interface PacketParser {
    fun classify(packet: ByteArray): PacketType
    fun isFirstVCL(packet: ByteArray): Boolean

    enum class PacketType {
        VCL,
        VPS,
        SPS,
        PPS,
        EOS,
        EOB,
        OTHER,
    }
}
