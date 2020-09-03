#pragma once

#include <string>
#include <string.h>
#include <iostream>
#include <cerrno>
#include <assert.h>
#include <stdexcept> // for runtime_error

#define DEBUG_BREAK_ONCE {static bool done=false;if(!done){ done=true;DebugBreak();}}
#ifndef TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_CHECKS 0
#endif
#define TELEPORT_COUT\
	std::cout<<__FILE__<<"("<<std::dec<<__LINE__<<"): info: "

#define TELEPORT_CERR\
	std::cerr<<__FILE__<<"("<<std::dec<<__LINE__<<"): warning: "

#define TELEPORT_ASSERT(c)\
	if(!c){TELEPORT_CERR<<"Assertion failed for "<<#c<<"\n";}

#if TELEPORT_INTERNAL_CHECKS
#define TELEPORT_INTERNAL_CERR\
		std::cerr << __FILE__ << "(" << __LINE__ << "): warning: "
#else
#define TELEPORT_INTERNAL_CERR\
	//
#endif