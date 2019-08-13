// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

//C Libraries
#include <iostream>
#include <string>
#include <assert.h>

//STL
#include <vector>
#include <deque>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>

//Debug
#define SCR_CERR_BREAK(msg, errCode) std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define SCR_CERR(msg)				 std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl;

#define SCR_COUT_BREAK(msg, errCode) std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define SCR_COUT(msg)				 std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl;

namespace scr
{
	class RenderPlatform;
	class APIObject
	{
	protected:
		RenderPlatform *renderPlatform;
		APIObject(RenderPlatform *r) :renderPlatform(r) {}
	};
}