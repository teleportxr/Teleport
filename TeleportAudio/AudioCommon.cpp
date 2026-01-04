#include "AudioCommon.h"

void log_print_impl(const char* source, const std::string& message)
{
	std::cout << source << " " << message << std::endl;
}