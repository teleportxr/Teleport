// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <cstdint>

class BitReader
{
public:
  BitReader(const uint8_t* data, size_t size);
  bool getBit();
  uint32_t getBits(size_t amount);
  void skipBits(size_t amount);
  uint32_t getGolombU();
  int32_t getGolombS();

  size_t getByteIndex() const { return mByteIndex; }

  size_t getSize() const { return mSize; }

  size_t getBitsRemaining() const;

private:
  const uint8_t* mData;
  size_t mSize;
  size_t mByteIndex;
  size_t mBitIndex;
};
