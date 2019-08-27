#include "Log.h"
#include <iostream>
#include <stdarg.h>

void ClientLog(const char* fileTag, int lineno, const char* msg_type, const char* format_str, ...)
{
	int size = (int)strlen(format_str) + 100;
	static std::string str;
	va_list ap;
	int n = -1;
	while (n < 0 || n >= size)
	{
		str.resize(size);
		va_start(ap, format_str);
		//n = vsnprintf_s((char *)str.c_str(), size, size,format_str, ap);
		n = vsnprintf((char*)str.c_str(), size, format_str, ap);
		va_end(ap);
		if (n > -1 && n < size)
		{
			str.resize(n);
		}
		if (n > -1)
			size = n + 1;
		else
			size *= 2;
	}
	std::cerr << fileTag << "(" << lineno << "): " << msg_type << ": " << str.c_str() << std::endl;
}
