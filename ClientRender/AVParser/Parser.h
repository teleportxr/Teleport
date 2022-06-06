// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <string>
#include <cstdint>

namespace avparser
{
  class Parser
  {
    public:
      virtual ~Parser();

      virtual size_t parseNALUnit(const uint8_t* data, std::size_t size) = 0;

      virtual void* getVPS() const { return nullptr;  };
      virtual void* getSPS() const = 0;
      virtual void* getPPS() const = 0;
      virtual void* getLastSlice() const = 0;
      virtual void* getExtraData() const = 0;
  };
}

