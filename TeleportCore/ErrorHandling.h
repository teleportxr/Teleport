#pragma once
#define WIN32_LEAN_AND_MEAN 
#include <string>
#include <string.h>
#include <iostream>
#include <cerrno>
#include <assert.h>
#include <stdexcept> // for runtime_error
#ifdef _MSC_VER
    #pragma warning(push)
	#pragma warning(disable:4996)
	#include <Windows.h>// for DebugBreak etc
#define DEBUG_BREAK_CROSS DebugBreak();
#elif defined __ANDROID__
#include <signal.h>
#define DEBUG_BREAK_CROSS raise(SIGTRAP);
#endif

#define DEBUG_BREAK_ONCE {static bool done=false;if(!done){ done=true;DEBUG_BREAK_CROSS}}
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
void TeleportLogUnsafe(const char* fmt, ...);
#define TELEPORT_INTERNAL_LOG_UNSAFE(...) \
    { TeleportLogUnsafe(__VA_ARGS__); }
#define TELEPORT_INTERNAL_CERR\
		std::cerr << __FILE__ << "(" << __LINE__ << "): warning: "
#else
#define TELEPORT_INTERNAL_CERR\
	//
#define TELEPORT_INTERNAL_LOG_UNSAFE(a,...)
#endif

#define TELEPORT_ASSERT(c)\
	if(!c){TELEPORT_CERR<<"Assertion failed for "<<#c<<"\n";}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif