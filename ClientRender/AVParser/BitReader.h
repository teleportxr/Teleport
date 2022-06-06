// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <cstdint>

class BitReader
{
public:
  BitReader(const uint8_t* data = nullptr, size_t size = 0);
  void start(const uint8_t* data, size_t size);
  bool getBit();
  uint32_t getBits(size_t amount);
  void skipBits(size_t amount);
  uint32_t getGolombU();
  int32_t getGolombS();

  // Return all bytes that have been fully or partially read.
  size_t getBytesRead() const;

  size_t getByteIndex() const { return mByteIndex; }

  size_t getSize() const { return mSize; }

  size_t getBitsRemaining() const;

private:
  const uint8_t* mData;
  size_t mSize;
  size_t mByteIndex;
  uint32_t mBitIndex;

  static constexpr uint32_t mBitStartIndex = 7;
};
