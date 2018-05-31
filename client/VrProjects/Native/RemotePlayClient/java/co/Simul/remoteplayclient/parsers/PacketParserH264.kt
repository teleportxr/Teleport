// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient.parsers

import java.nio.ByteBuffer

class PacketParserH264 : PacketParser {
    override fun classify(packet: ByteArray): PacketParser.PacketType {
        val header = packet[0].toInt()
        return when(header and 0x1F) {
            in 1..5 -> PacketParser.PacketType.VCL
            7    -> PacketParser.PacketType.SPS
            8    -> PacketParser.PacketType.PPS
            10   -> PacketParser.PacketType.EOS
            11   -> PacketParser.PacketType.EOB
            else -> PacketParser.PacketType.OTHER
        }
    }
    override fun isFirstVCL(packet: ByteArray): Boolean {
        val header = packet[0].toInt()
        return when(header and 0x1F) {
            1,2,5 -> ByteBuffer.wrap(packet).short.toInt() == 0
            else  -> false
        }
    }
}