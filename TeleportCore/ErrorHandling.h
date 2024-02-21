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

#if TELEPORT_INTERNAL_CHECKS
#include <fmt/core.h>
#endif
#ifdef _MSC_VER
    #pragma warning(push)
	#pragma warning(disable:4996)
#endif
namespace teleport
{
	extern void DebugBreak();
#if TELEPORT_INTERNAL_CHECKS
	template<typename... Args> void InternalWarn(const char *txt, Args... args)
	{
		std::cerr<<fmt::format(txt,args...).c_str() << "\n";
	}
	template<typename... Args> void InternalInfo(const char *txt, Args...args)
	{
		std::cout<<fmt::format(txt,args...).c_str() << "\n";
	}
#else
	template<typename... Args> void InternalWarn(const char *, Args... )
	{
	}
	template<typename... Args> void InternalInfo(const char *, Args... )
	{
	}
#endif
}
#define DEBUG_BREAK_ONCE {static bool done=false;if(!done){ done=true;teleport::DebugBreak();}}

#ifndef TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_CHECKS 0
#endif

#if TELEPORT_INTERNAL_CHECKS
#define TELEPORT_LOG(txt, ...)\
	teleport::InternalInfo((fmt::format("{0} ({1}): info: ", __FILE__,__LINE__)+#txt).c_str(),##__VA_ARGS__)
#else
#define TELEPORT_LOG(txt, ...)
#endif

#define TELEPORT_COUT\
	std::cout<<__FILE__<<"("<<std::dec<<__LINE__<<"): info: "

#define TELEPORT_CERR\
	std::cerr<<__FILE__<<"("<<std::dec<<__LINE__<<"): warning: "

#define TELEPORT_WARN(txt, ...)\
	teleport::InternalWarn((fmt::format("{0} ({1}): warning: ", __FILE__, __LINE__) + #txt).c_str(), ##__VA_ARGS__)

#define TELEPORT_CERR_BREAK(msg, errCode) std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define TELEPORT_COUT_BREAK(msg, errCode) std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);

#define TELEPORT_BREAK_ONCE(msg) {TELEPORT_CERR<<msg<<std::endl;DEBUG_BREAK_ONCE}

#if TELEPORT_INTERNAL_CHECKS
	#define TELEPORT_INTERNAL_BREAK_ONCE(txt, ...) {teleport::InternalWarn((fmt::format("{0} ({1}): error: ", __FILE__,__LINE__)+#txt).c_str(),##__VA_ARGS__);DEBUG_BREAK_ONCE}
		void TeleportLogUnsafe(const char* fmt, ...);
	#define TELEPORT_INTERNAL_LOG_UNSAFE(...) \
		{ TeleportLogUnsafe(__VA_ARGS__); }
	#define TELEPORT_INTERNAL_CERR(txt, ...) teleport::InternalWarn((fmt::format("{0} ({1}): warning: ", __FILE__,__LINE__)+#txt).c_str(),##__VA_ARGS__)
	#define TELEPORT_INTERNAL_COUT(txt, ...) teleport::InternalInfo((fmt::format("{0} ({1}): info: ", __FILE__,__LINE__)+#txt).c_str(),##__VA_ARGS__)

#else
	#define TELEPORT_INTERNAL_BREAK_ONCE(txt, ...)
	#define TELEPORT_INTERNAL_LOG_UNSAFE(...)
	#define TELEPORT_INTERNAL_CERR(txt, ...)
	#define TELEPORT_INTERNAL_COUT(txt, ...)
#endif
#define TELEPORT_ASSERT(c)\
	if(!(c)){TELEPORT_CERR<<"Assertion failed for "<<#c<<"\n";}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif