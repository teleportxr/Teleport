#pragma once
#include <string>
#if LIBAV_USE_SRT
#include <srt.h>
namespace avs
{
	extern sockaddr_in CreateAddrInet(const std::string& name, unsigned short port);
	extern void CHECK_SRT_ERROR(int err);
}
#endif