#include "SmokeLib.h"

#include <cstdio>

int main()
{
	std::printf("%s\n", smoke::Describe(42).c_str());
	return 0;
}
