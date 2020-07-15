// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <string>

namespace avs::test {

class Utility
{
public:
	static std::string readTextFile(const char* filename);
};

} // avs::test