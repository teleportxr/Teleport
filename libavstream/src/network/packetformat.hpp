// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <memory>
#include <vector>
#include <libavstream/common.hpp>
#include <util/binaryio.hpp>

namespace avs
{

	namespace PacketFormat
	{
		static constexpr size_t HeaderSize = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t);
		static constexpr size_t HeaderSize_FirstFragment = HeaderSize +  sizeof(uint32_t);
		static constexpr size_t HeaderSize_LastFragment =  HeaderSize_FirstFragment;
#if LIBAV_USE_SRT
		static constexpr size_t MaxPacketSize = 1450;
#else
		static constexpr size_t MaxPacketSize = 1450;
#endif
		static constexpr size_t MaxPayloadSize = MaxPacketSize - HeaderSize;
		static constexpr size_t MaxPayloadSize_FirstFragment = MaxPacketSize - HeaderSize_FirstFragment;
		static constexpr size_t MaxPayloadSize_LastFragment = MaxPacketSize - HeaderSize_LastFragment;

		static constexpr int    MaxNumStreams = 256;
	}

	struct NetworkPacket
	{
		// Header
		uint16_t sequence;
		struct
		{
			bool fragmentFirst;
			bool fragmentLast;
			bool frameFirst;
			bool frameLast;
		} flags;

		// Fragment header
		uint8_t  streamIndex;
		uint32_t timestamp;

		// Payload
		ByteBuffer payload;

		//! Send this packet to the buffer.
		void serialize(ByteBuffer& buffer) const;
		bool unserialize(const ByteBuffer& buffer, size_t offset, size_t numBytes);

		bool isFragmented() const
		{
			return !(flags.fragmentFirst && flags.fragmentLast);
		}

		unsigned long long checksum;
	};

	inline void NetworkPacket::serialize(ByteBuffer& buffer) const
	{
		uint8_t encodedFlags = 0;
		encodedFlags |= (flags.fragmentFirst) ? 0x01 : 0;
		encodedFlags |= (flags.fragmentLast) ? 0x02 : 0;
		encodedFlags |= (flags.frameFirst) ? 0x04 : 0;
		encodedFlags |= (flags.frameLast) ? 0x08 : 0;

		BinaryWriter writer(buffer);
		writer.put<uint16_t>(sequence);					//2
		writer.put<uint8_t>(encodedFlags);				//1
		writer.put<uint8_t>(streamIndex);				//1
		if (flags.fragmentFirst||flags.fragmentLast)
		{
			writer.put<uint32_t>(timestamp);			//4
		}

		buffer.insert(std::end(buffer), std::begin(payload), std::end(payload));
	}

	inline bool NetworkPacket::unserialize(const ByteBuffer& buffer, size_t offset, size_t numBytes)
	{
		if (numBytes < PacketFormat::HeaderSize||numBytes>PacketFormat::MaxPacketSize)
		{
			AVSLOG(Warning) << "Failed to unserialize packet," << "\n";
			return false;
		}
		BinaryReader reader(buffer,offset);

		sequence = reader.get<uint16_t>();
		const uint8_t encodedFlags = reader.get<uint8_t>();

		flags.fragmentFirst = (encodedFlags & 0x01) != 0;
		flags.fragmentLast = (encodedFlags & 0x02) != 0;
		flags.frameFirst = (encodedFlags & 0x04) != 0;
		flags.frameLast = (encodedFlags & 0x08) != 0;

		streamIndex = 0;
		timestamp = 0;
		streamIndex = reader.get<uint8_t>();

		assert(streamIndex == 50 || streamIndex == 100);// Temp check for bad data.

		if (flags.fragmentFirst||flags.fragmentLast)
		{
            if (flags.fragmentFirst&&(numBytes < PacketFormat::HeaderSize_FirstFragment))
			{
				AVSLOG(Warning) << "Failed to unserialize packet," << "\n";
				return false;
			}
			if (flags.fragmentLast && (numBytes < PacketFormat::HeaderSize_LastFragment))
			{
				AVSLOG(Warning) << "Failed to unserialize packet," << "\n";
				return false;
			}
		}
		if (flags.fragmentFirst||flags.fragmentLast)
			timestamp = reader.get<uint32_t>();

		if (reader.offset() < numBytes)
		{
			// std::vector::assign copies a set of values, i.e. a chunk of memory,
			// in the case copying from the current position in buffer to the end into the payload.
			payload.assign(std::begin(buffer) + reader.offset(), std::begin(buffer) + numBytes);
		}
		return true;
	}

} // avs