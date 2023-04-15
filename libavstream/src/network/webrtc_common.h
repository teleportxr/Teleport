#pragma once
#if GOOGLE_WEBRTC
#include <api/scoped_refptr.h>
namespace webrtc
{
	class PeerConnectionFactoryInterface;
}
namespace avs
{
	extern rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> g_peer_connection_factory;
	extern void CreatePeerConnectionFactory();
}
#endif
namespace avs
{
	extern const char *iceServers[];
};