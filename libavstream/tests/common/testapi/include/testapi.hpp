// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <cassert>
#include <stdexcept>
#include <iostream>
#include <memory>

#include "statistics.hpp"

namespace avs::test {

class Test
{
public:
	virtual ~Test() = default;
	virtual bool parseOptions(int argc, char* argv[]) { return true; };
	virtual void run() = 0;
};

static inline int main(int argc, char* argv[], Test* _test)
{
	assert(_test);
	try {
		std::unique_ptr<Test> test{ _test };
		if(!test->parseOptions(argc, argv)) {
			return 1;
		}
		test->run();
	}
	catch(const std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
	return 0;
}

} // avs::test

#define IMPLEMENT_TEST(TestClassName) \
	int main(int argc, char* argv[]) { return avs::test::main(argc, argv, new TestClassName); }
