// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include "network/webrtc_common.h"
#if GOOGLE_WEBRTC
#include <api/peer_connection_interface.h>
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/audio_codecs/audio_decoder_factory_template.h>
#include <api/audio_codecs/audio_encoder_factory_template.h>
#include <api/audio_codecs/opus_audio_encoder_factory.h>
#include <api/audio_codecs/opus_audio_decoder_factory.h>
#include <media/engine/multiplex_codec_factory.h>
#include <media/engine/internal_encoder_factory.h>
#include <media/engine/internal_decoder_factory.h>
#include <rtc_base/physical_socket_server.h>
#include <api/audio_codecs/g711/audio_decoder_g711.h>
#include <api/audio_codecs/g711/audio_encoder_g711.h>

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> avs::g_peer_connection_factory;

// The socket that the signaling thread and worker thread communicate on.
rtc::PhysicalSocketServer socket_server;
// The separate thread where all of the WebRTC code runs
static std::unique_ptr<rtc::Thread> g_worker_thread;
static std::unique_ptr<rtc::Thread> g_signaling_thread;
void avs::CreatePeerConnectionFactory()
{
	if (g_peer_connection_factory == nullptr)
	{
		rtc::LogMessage::LogToDebug(rtc::LS_INFO);
		g_worker_thread = rtc::Thread::Create();
		// previously: g_worker_thread.reset(new rtc::Thread(&socket_server));
		// Doe we need this "socket server"?
		g_worker_thread->Start();
		g_signaling_thread = rtc::Thread::Create();
		g_signaling_thread->Start();
		// No audio or video decoders: for now, all streams will be treated as plain data.
		// But WebRTC requires non-null factories:
		g_peer_connection_factory = webrtc::CreatePeerConnectionFactory(
			g_worker_thread.get(), g_worker_thread.get(), g_signaling_thread.get(),
			nullptr,
			webrtc::CreateBuiltinAudioEncoderFactory(),
			webrtc::CreateBuiltinAudioDecoderFactory(),
			std::unique_ptr<webrtc::VideoEncoderFactory>(new webrtc::MultiplexEncoderFactory(absl::make_unique<webrtc::InternalEncoderFactory>())),
			std::unique_ptr<webrtc::VideoDecoderFactory>(new webrtc::MultiplexDecoderFactory(absl::make_unique<webrtc::InternalDecoderFactory>())),
			nullptr, nullptr);
	}
}
#endif

// A small list of currently-functional public STUN servers. Entries must be
// fully-qualified URLs (stun:/turn:/turns:); libdatachannel's IceServer URL
// parser will misinterpret bare "host:port" strings (treating leading
// hostname components as the scheme) and throw, aborting ICE configuration.
// For TURN, configure your own credentialed server(s) via SetIceServers
// rather than adding them here.
const char *avs::iceServers[]={
    "stun:stun.l.google.com:19302",
    "stun:stun1.l.google.com:19302",
    "stun:stun2.l.google.com:19302",
    "stun:stun3.l.google.com:19302",
    "stun:stun4.l.google.com:19302",
    "stun:stun.cloudflare.com:3478",
    "stun:stun.nextcloud.com:443"
	,nullptr};