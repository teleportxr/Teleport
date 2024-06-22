#pragma once
#include <string>
#include <string.h>
#include <iostream>
#include <fmt/core.h>

#ifdef _MSC_VER
    #pragma warning(push)
	#pragma warning(disable:4996)
#endif
namespace teleport
{
	template <typename... Args>
	void Warn(const char *file, int line, const char *function,const char *txt, Args... args)
	{
		std::string str = fmt::format("{0} ({1}): warning: {2}: {3}", file, line,function, txt);
		std::cerr << fmt::format(str, args...).c_str() << "\n";
	}
	template <typename... Args>
	void Info(const char *file, int line, const char *function,const char *txt, Args... args)
	{
		std::string str = fmt::format("{0} ({1}): info: {2}: {3}", file, line,function, txt);
		std::cout << fmt::format(str, args...).c_str() << "\n";
	}
}

#define TELEPORT_WARN(txt, ...)\
	teleport::Warn(__FILE__, __LINE__, __func__, #txt, ##__VA_ARGS__)

#define TELEPORT_WARN_NOSPAM(txt, ...) \
	{\
		static uint8_t count=3;\
		if(count>0)\
		{\
			count--;\
			teleport::Warn(__FILE__, __LINE__, __func__,#txt, ##__VA_ARGS__);\
		}\
	}

#define TELEPORT_INFO(txt, ...) \
	teleport::Info(__FILE__, __LINE__,__func__,#txt,##__VA_ARGS__)
#ifdef _MSC_VER
    #pragma warning(pop)
#endif