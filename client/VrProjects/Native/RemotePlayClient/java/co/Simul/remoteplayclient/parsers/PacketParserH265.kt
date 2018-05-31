// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient.parsers

class PacketParserH265: PacketParser {
    override fun classify(packet: ByteArray): PacketParser.PacketType {
        val header = packet[0].toInt()
        return when((header and 0x7E) shr 1) {
            in 0..31 -> PacketParser.PacketType.VCL
            32   -> PacketParser.PacketType.VPS
            33   -> PacketParser.PacketType.SPS
            34   -> PacketParser.PacketType.PPS
            36   -> PacketParser.PacketType.EOS
            37   -> PacketParser.PacketType.EOB
            else -> PacketParser.PacketType.OTHER
        }
    }

    override fun isFirstVCL(packet: ByteArray): Boolean {
        return (packet[2].toInt() and 0x80) > 0
    }
}