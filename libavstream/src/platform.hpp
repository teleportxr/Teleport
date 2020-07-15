// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#if defined(PLATFORM_WINDOWS)
	#include <libavstream/platforms/platform_windows.hpp>
#else
	#include <libavstream/platforms/platform_posix.hpp>
#endif
