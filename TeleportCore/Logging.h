#pragma once
#include <string>
#include <string.h>
#include <iostream>

#if __cplusplus>=202002L
#include <format>
#else
#include <fmt/core.h>
#endif
#ifdef _MSC_VER
    #pragma warning(push)
	#pragma warning(disable:4996)
#endif
namespace teleport
{
	enum class LogPriority
	{
		UNKNOWN,
		DEFAULT,
		VERBOSE,
		LOG_DEBUG,
		INFO,
		WARNING,
		LOG_ERROR,
		FATAL,
		SILENT
	};
#if __cplusplus>=202002L
	template <typename... Args>
	void Warn(const char *file, int line, const char *function,const std::format_string<Args...> txt, Args&&... args)
	{
		std::string str1 = std::format("{} ({}): warning: {}: ", file, line,function);
		std::string str2 = std::vformat(txt.get(), std::make_format_args(args...) );
		std::cerr << str1<<str2 << "\n";
	}
	template <typename... Args>
	void Info(const char *file, int line, const char *function,const std::format_string<Args...> txt, Args... args)
	{
		std::string str1 = std::format("{} ({}): info: {}: ", file, line,function);
		std::string str2 = std::vformat(txt.get(), std::make_format_args(args...) );
		std::cerr << str1<<str2 << "\n";
	}
#else
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
#endif
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

#define TELEPORT_LOG(txt, ...) \
	teleport::Info(__FILE__, __LINE__,__func__,#txt,##__VA_ARGS__)

#define TELEPORT_INFO(txt, ...) \
	teleport::Info(__FILE__, __LINE__,__func__,#txt,##__VA_ARGS__)

	
#if TELEPORT_INTERNAL_CHECKS
#define TELEPORT_LOG_INTERNAL(txt, ...) \
	teleport::Info(__FILE__, __LINE__,__func__,#txt,##__VA_ARGS__)
#define TELEPORT_WARN_INTERNAL(txt, ...)\
	teleport::Warn(__FILE__, __LINE__, __func__, #txt, ##__VA_ARGS__)
#else
#define TELEPORT_LOG_INTERNAL(txt, ...) 
#define TELEPORT_WARN_INTERNAL(txt, ...)
#endif
#ifdef _MSC_VER
    #pragma warning(pop)
#endif