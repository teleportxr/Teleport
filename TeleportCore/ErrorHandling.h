#pragma once
#define WIN32_LEAN_AND_MEAN 
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
#define TELEPORT_COUT\
	std::cout<<__FILE__<<"("<<std::dec<<__LINE__<<"): info: "

#define TELEPORT_CERR\
	std::cerr<<__FILE__<<"("<<std::dec<<__LINE__<<"): warning: "
	
#define TELEPORT_WARN\
	std::cerr<<__FILE__<<"("<<std::dec<<__LINE__<<"): warning: "


#define TELEPORT_CERR_BREAK(msg, errCode) std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define TELEPORT_COUT_BREAK(msg, errCode) std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);

#define TELEPORT_BREAK_ONCE(msg) {TELEPORT_CERR<<msg<<std::endl;DEBUG_BREAK_ONCE}

#if TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_BREAK_ONCE(msg) {TELEPORT_CERR<<msg<<std::endl;DEBUG_BREAK_ONCE}
void TeleportLogUnsafe(const char* fmt, ...);
#define TELEPORT_INTERNAL_LOG_UNSAFE(...) \
    { TeleportLogUnsafe(__VA_ARGS__); }
#else
#define TELEPORT_INTERNAL_BREAK_ONCE(msg)
#define TELEPORT_INTERNAL_LOG_UNSAFE(a,...)
#endif
#define TELEPORT_INTERNAL_CERR(txt, ...) teleport::InternalWarn("{0} ({1}): warning: " #txt, __FILE__,__LINE__,##__VA_ARGS__)
#define TELEPORT_INTERNAL_COUT(txt, ...) teleport::InternalInfo("{0} ({1}): info: " #txt, __FILE__,__LINE__,##__VA_ARGS__)

#define TELEPORT_ASSERT(c)\
	if(!c){TELEPORT_CERR<<"Assertion failed for "<<#c<<"\n";}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif