#pragma once

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
#else
inline void DebugBreak(){}
#endif

#define DEBUG_BREAK_ONCE {static bool done=false;if(!done){ done=true;DebugBreak();}}
#ifndef TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_CHECKS 0
#endif
#define TELEPORT_COUT\
	std::cout<<__FILE__<<"("<<std::dec<<__LINE__<<"): info: "

#define TELEPORT_CERR\
	std::cerr<<__FILE__<<"("<<std::dec<<__LINE__<<"): warning: "

#define TELEPORT_BREAK_ONCE(msg) {TELEPORT_CERR<<msg<<std::endl;DEBUG_BREAK_ONCE}

#if TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_CERR\
		std::cerr << __FILE__ << "(" << __LINE__ << "): warning: "
#else
#define TELEPORT_INTERNAL_CERR\
	//
#endif
#ifdef _MSC_VER
    #pragma warning(pop)
#endif