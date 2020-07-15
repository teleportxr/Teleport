#include "util/srtutil.h"

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

        // XXX RACY!!! Use getaddrinfo() instead. Check portability.
        // Windows/Linux declare it.
        // See:
        //  http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancedInternet3b.html
        hostent* he = gethostbyname(name.c_str());
        if ( !he || he->h_addrtype != AF_INET )
            throw std::invalid_argument("SrtSource: host not found: " + name);

        sa.sin_addr = *(in_addr*)he->h_addr_list[0];
    }

    return sa;
}

void avs::CHECK_SRT_ERROR(int err)
{
	if(!err)
		return;
	const char* errstr=srt_getlasterror_str();
	std::cerr<<"Srt: error: "<<(errstr?errstr:"unknown")<<std::endl;
}
#endif