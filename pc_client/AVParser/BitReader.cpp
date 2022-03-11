// (C) Copyright 2018-2022 Simul Software Ltd

#include "BitReader.h"

#include <climits>
#include <cassert>
#include "ErrorHandling.h"

BitReader::BitReader(const uint8_t* data, size_t size) 
	: mData(data)
	, mSize(size)
	, mByteIndex(0)
	, mBitIndex(CHAR_BIT - 1)
{
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

	if (mBitIndex > CHAR_BIT)
	{
		mBitIndex = CHAR_BIT - 1;
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


uint32_t BitReader::getBits(size_t amount)
{
	uint32_t result = 0;
	for (size_t i = 0; i < amount; i++)
	{
		if (getBit())
		{
			result |= 1 << (amount - i - 1);
		}
	}

	return result;
}


void BitReader::skipBits(size_t amount)
{
	if (mByteIndex >= 2)
	{
		if (mData[mByteIndex - 2] == 0 && mData[mByteIndex - 1] == 0 && mData[mByteIndex] == 3)
		{
			mByteIndex++;
		}
	}

	uint32_t bytesToSkip = amount / 8;


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

	if (mBitIndex > amount % 8)
	{
		mBitIndex -= amount % 8;
	}
	else
	{
		mByteIndex++;
		mBitIndex = mBitIndex - (amount % 8) + 8;
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

size_t BitReader::getBitsRemaining() const
{
	size_t numBytesRemaining = mSize - mByteIndex - 1;
	return (numBytesRemaining * CHAR_BIT) + mBitIndex + 1;
}

