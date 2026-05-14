#pragma once
#ifdef _MSC_VER
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN 
	#endif
#endif
#include <string>
#include <string.h>
#include <iostream>
#include <cerrno>
#include <assert.h>
#include <stdexcept> // for runtime_error
#include <format>

#if TELEPORT_INTERNAL_CHECKS
#ifdef check
#undef TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_CHECKS 0
#endif
#endif
#ifdef _MSC_VER
    #pragma warning(push)
	#pragma warning(disable:4996)
#endif
namespace teleport
{
	extern void DebugBreak();

	//! Logging categories for TELEPORT_INTERNAL_COUT. Each category may be enabled or disabled at runtime.
	enum class LogCategory
	{
		Default,
		Time,
		Resource,
	};

	extern bool IsLogCategoryEnabled(LogCategory category);
	extern void SetLogCategoryEnabled(LogCategory category, bool enabled);

#if TELEPORT_INTERNAL_CHECKS
	template <typename... Args>
	void InternalWarn(const char *file, int line, const char *function,const std::format_string<Args...> txt, Args&&... args)
	{
		std::string str1 = std::format("{} ({}): warning: {}: ", file, line,function);
		std::string str2 = std::vformat(txt.get(), std::make_format_args(args...) );
		std::cerr << str1<<str2 << "\n";
	}
	template <typename... Args>
	void InternalInfo(const char *file, int line, const char *function,const std::format_string<Args...> txt, Args... args)
	{
		std::string str1 = std::format("{} ({}): info: {}: ", file, line,function);
		std::string str2 = std::vformat(txt.get(), std::make_format_args(args...) );
		std::cerr << str1<<str2 << "\n";
	}
#else
	template<typename... Args> void InternalWarn(const char *,int,const char *,const char *, Args... )
	{
	}
	template<typename... Args> void InternalInfo(const char *,int,const char *,const char *, Args... )
	{
	}
#endif
}
#define DEBUG_BREAK_ONCE {static bool done=false;if(!done){ done=true;teleport::DebugBreak();}}

#ifndef TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_CHECKS 0
#endif


#define TELEPORT_COUT\
	std::cout<<__FILE__<<"("<<std::dec<<__LINE__<<"): info: "

#define TELEPORT_CERR\
	std::cerr<<__FILE__<<"("<<std::dec<<__LINE__<<"): warning: "

#define TELEPORT_CERR_BREAK(msg, errCode) std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define TELEPORT_COUT_BREAK(msg, errCode) std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);

#define TELEPORT_BREAK_ONCE(msg) {teleport::InternalWarn( __FILE__,__LINE__, __func__, msg); DEBUG_BREAK_ONCE}

#if TELEPORT_INTERNAL_CHECKS
	#define TELEPORT_INTERNAL_BREAK_ONCE(txt, ...) {teleport::InternalWarn( __FILE__,__LINE__, __func__, txt, ##__VA_ARGS__);DEBUG_BREAK_ONCE}
		void TeleportLogUnsafe(const char* fmt, ...);
	#define TELEPORT_INTERNAL_LOG_UNSAFE(...) \
		{ TeleportLogUnsafe(__VA_ARGS__); }
	#define TELEPORT_INTERNAL_CERR(txt, ...) teleport::InternalWarn(__FILE__,__LINE__, __func__, txt, ##__VA_ARGS__)
	#define TELEPORT_INTERNAL_COUT(category, txt, ...) \
		do { if (teleport::IsLogCategoryEnabled(teleport::LogCategory::category)) \
			teleport::InternalInfo(__FILE__,__LINE__, __func__, txt, ##__VA_ARGS__); } while(0)

#else
	#define TELEPORT_INTERNAL_BREAK_ONCE(txt, ...)
	#define TELEPORT_INTERNAL_LOG_UNSAFE(...)
	#define TELEPORT_INTERNAL_CERR(txt, ...)
	#define TELEPORT_INTERNAL_COUT(category, txt, ...)
#endif
#define TELEPORT_ASSERT(c)\
	if(!(c)){TELEPORT_CERR<<"Assertion failed for "<<#c<<"\n";}
	

constexpr inline const char * const verify_failed="Verify failed for {0}";

#define VERIFY_EQUALITY_CHECK(a, b)          \
if (b != a.b)                                 \
{                                             \
	TELEPORT_WARN(verify_failed, #b); \
	return false;                             \
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif