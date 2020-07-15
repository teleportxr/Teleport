// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <stdexcept>
#include <fstream>
#include <sstream>

#include <util.hpp>

namespace avs::test {

std::string Utility::readTextFile(const char* filename)
{
	std::ifstream file{filename};
	if(!file.is_open()) {
		throw std::runtime_error("Could not open file: " + std::string{ filename });
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

} // avs::test
