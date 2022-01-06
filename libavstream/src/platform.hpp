// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#if defined(_MSC_VER)
	#include <libavstream/platforms/platform_windows.hpp>
#else
	#include <libavstream/platforms/platform_posix.hpp>
#endif
