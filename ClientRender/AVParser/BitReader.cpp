// (C) Copyright 2018-2022 Simul Software Ltd

#include "BitReader.h"

#include <climits>
#include <cassert>
#include "TeleportCore/ErrorHandling.h"

BitReader::BitReader(const uint8_t* data, size_t size) 
{
	start(data, size);
}

void BitReader::start(const uint8_t* data, size_t size)
{
	mData = data;
	mSize = size;
	mByteIndex = 0;
	mBitIndex = mBitStartIndex;
}

bool BitReader::getBit()
{
	if (mByteIndex >= mSize)
	{
		TELEPORT_CERR << "Not enough data" << std::endl;
		return false;
	}

	bool res = mData[mByteIndex] & (1 << mBitIndex);

	mBitIndex--;

	// The unsigned value of mBitIndex will become UINT32_MAX if it goes below 0.
	if (mBitIndex > mBitStartIndex)
	{
		mBitIndex = mBitStartIndex;
		mByteIndex++;

		if (mByteIndex >= 2)
		{
			if (mData[mByteIndex - 2] == 0 && mData[mByteIndex - 1] == 0 && mData[mByteIndex] == 3)
			{
				mByteIndex++;
			}
		}
	}

	return res;
}


uint32_t BitReader::getBits(size_t count)
{
	uint32_t result = 0;
	for (size_t i = 0; i < count; i++)
	{
		if (getBit())
		{
			result |= 1 << (count - i - 1);
		}
	}

	return result;
}


void BitReader::skipBits(size_t count)
{
	if (mByteIndex >= 2)
	{
		if (mData[mByteIndex - 2] == 0 && mData[mByteIndex - 1] == 0 && mData[mByteIndex] == 3)
		{
			mByteIndex++;
		}
	}

	uint32_t bytesToSkip = uint32_t(count)/ 8;


	while (bytesToSkip)
	{
		bytesToSkip--;
		mByteIndex++;
		if (mByteIndex >= 2)
		{
			if (mData[mByteIndex - 2] == 0 && mData[mByteIndex - 1] == 0 && mData[mByteIndex] == 3)
			{
				mByteIndex++;
			}
		}
	}

	if (mBitIndex > count % 8)
	{
		mBitIndex -= count % 8;
	}
	else
	{
		mByteIndex++;
		mBitIndex = mBitIndex - (count % 8) + 8;
	}

}

uint32_t BitReader::getGolombU()
{
	long zeroBitCount = -1;

	for (long bit = 0; !bit; zeroBitCount++)
	{
		bit = getBit();
	}

	if (zeroBitCount >= 32)
	{
		return 0;
	}

	return (1 << zeroBitCount) - 1 + getBits(zeroBitCount);
}



int32_t BitReader::getGolombS()
{
	int32_t buffer = getGolombU();

	if (buffer & 1)
	{
		buffer = (buffer + 1) >> 1;
	}
	else
	{
		buffer = -(buffer >> 1);
	}

	return buffer;
}

size_t BitReader::getBytesRead() const
{
	if (mBitIndex == mBitStartIndex)
	{
		return mByteIndex;
	}
	return mByteIndex + 1;
}

size_t BitReader::getBitsRemaining() const
{
	size_t numBytesRemaining = (mSize - mByteIndex) - 1;
	return (numBytesRemaining * 8) + mBitIndex + 1;
}

