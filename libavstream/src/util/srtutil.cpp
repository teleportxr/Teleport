#include "util/srtutil.h"
#include "logger.hpp"

#if LIBAV_USE_SRT
using namespace avs;

sockaddr_in avs::CreateAddrInet(const std::string& name, unsigned short port)
{
    sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    if ( name != "" )
    {
        if ( inet_pton(AF_INET, name.c_str(), &sa.sin_addr) == 1 )
            return sa;
#ifdef _MSC_VER
        struct addrinfo* res;
        struct in_addr addr;
        getaddrinfo(name.c_str(), NULL, 0, &res);
        if (!res)
        {
            throw std::runtime_error("avs::CreateAddrInet: " + name);
        }
        addr.S_un = ((struct sockaddr_in*)(res->ai_addr))->sin_addr.S_un;
        sa.sin_addr.s_addr = inet_addr(inet_ntoa(addr));
#else
        // XXX RACY!!! Use getaddrinfo() instead. Check portability.
        // Windows/Linux declare it.
        // See:
        //  http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancedInternet3b.html
        hostent* he = gethostbyname(name.c_str());

        if ( !he || he->h_addrtype != AF_INET )
            throw std::invalid_argument("SrtSource: host not found: " + name);

        sa.sin_addr = *(in_addr*)he->h_addr_list[0];
#endif
    }

    return sa;
}

void avs::CHECK_SRT_ERROR(int err)
{
	if(!err)
		return;
	const char* errstr=srt_getlasterror_str();
    AVSLOG(Error) << "Srt: error: " << (errstr ? errstr : "unknown") << "\n";
}
#endif