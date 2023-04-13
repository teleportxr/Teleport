// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

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

const char *avs::iceServers[]={
    "iphone-stun.strato-iphone.de:3478",
    "numb.viagenie.ca:3478",
    "stun.12connect.com:3478",
    "stun.12voip.com:3478",
    "stun.1und1.de:3478",
    "stun.3cx.com:3478",
    "stun.acrobits.cz:3478",
    "stun.actionvoip.com:3478",
    "stun.advfn.com:3478",
    "stun.altar.com.pl:3478",
    "stun.antisip.com:3478",
    "stun.avigora.fr:3478",
    "stun.bluesip.net:3478",
    "stun.cablenet-as.net:3478",
    "stun.callromania.ro:3478",
    "stun.callwithus.com:3478",
    "stun.cheapvoip.com:3478",
    "stun.cloopen.com:3478",
    "stun.commpeak.com:3478",
    "stun.cope.es:3478",
    "stun.counterpath.com:3478",
    "stun.counterpath.net:3478",
    "stun.dcalling.de:3478",
    "stun.demos.ru:3478",
    "stun.dus.net:3478",
    "stun.easycall.pl:3478",
    "stun.easyvoip.com:3478",
    "stun.ekiga.net:3478",
    "stun.epygi.com:3478",
    "stun.etoilediese.fr:3478",
    "stun.faktortel.com.au:3478",
    "stun.freecall.com:3478",
    "stun.freeswitch.org:3478",
    "stun.freevoipdeal.com:3478",
    "stun.gmx.de:3478",
    "stun.gmx.net:3478",
    "stun.halonet.pl:3478",
    "stun.hoiio.com:3478",
    "stun.hosteurope.de:3478",
    "stun.infra.net:3478",
    "stun.internetcalls.com:3478",
    "stun.intervoip.com:3478",
    "stun.ipfire.org:3478",
    "stun.ippi.fr:3478",
    "stun.ipshka.com:3478",
    "stun.it1.hr:3478",
    "stun.ivao.aero:3478",
    "stun.jumblo.com:3478",
    "stun.justvoip.com:3478",
    "stun.l.google.com:19302",
    "stun.linphone.org:3478",
    "stun.liveo.fr:3478",
    "stun.lowratevoip.com:3478",
    "stun.lundimatin.fr:3478",
    "stun.mit.de:3478",
    "stun.miwifi.com:3478",
    "stun.modulus.gr:3478",
    "stun.myvoiptraffic.com:3478",
    "stun.netappel.com:3478",
    "stun.netgsm.com.tr:3478",
    "stun.nfon.net:3478",
    "stun.nonoh.net:3478",
    "stun.nottingham.ac.uk:3478",
    "stun.ooma.com:3478",
    "stun.ozekiphone.com:3478",
    "stun.pjsip.org:3478",
    "stun.poivy.com:3478",
    "stun.powervoip.com:3478",
    "stun.ppdi.com:3478",
    "stun.qq.com:3478",
    "stun.rackco.com:3478",
    "stun.rockenstein.de:3478",
    "stun.rolmail.net:3478",
    "stun.rynga.com:3478",
    "stun.schlund.de:3478",
    "stun.sigmavoip.com:3478",
    "stun.sip.us:3478",
    "stun.sipdiscount.com:3478",
    "stun.sipgate.net:10000",
    "stun.sipgate.net:3478",
    "stun.siplogin.de:3478",
    "stun.sipnet.net:3478",
    "stun.sipnet.ru:3478",
    "stun.sippeer.dk:3478",
    "stun.siptraffic.com:3478",
    "stun.sma.de:3478",
    "stun.smartvoip.com:3478",
    "stun.smsdiscount.com:3478",
    "stun.solcon.nl:3478",
    "stun.solnet.ch:3478",
    "stun.sonetel.com:3478",
    "stun.sonetel.net:3478",
    "stun.sovtest.ru:3478",
    "stun.srce.hr:3478",
    "stun.stunprotocol.org:3478",
    "stun.t-online.de:3478",
    "stun.tel.lu:3478",
    "stun.telbo.com:3478",
    "stun.tng.de:3478",
    "stun.twt.it:3478",
    "stun.uls.co.za:3478",
    "stun.unseen.is:3478",
    "stun.usfamily.net:3478",
    "stun.viva.gr:3478",
    "stun.vivox.com:3478",
    "stun.vo.lu:3478",
    "stun.voicetrading.com:3478",
    "stun.voip.aebc.com:3478",
    "stun.voip.blackberry.com:3478",
    "stun.voip.eutelia.it:3478",
    "stun.voipblast.com:3478",
    "stun.voipbuster.com:3478",
    "stun.voipbusterpro.com:3478",
    "stun.voipcheap.co.uk:3478",
    "stun.voipcheap.com:3478",
    "stun.voipgain.com:3478",
    "stun.voipgate.com:3478",
    "stun.voipinfocenter.com:3478",
    "stun.voipplanet.nl:3478",
    "stun.voippro.com:3478",
    "stun.voipraider.com:3478",
    "stun.voipstunt.com:3478",
    "stun.voipwise.com:3478",
    "stun.voipzoom.com:3478",
    "stun.voys.nl:3478",
    "stun.voztele.com:3478",
    "stun.webcalldirect.com:3478",
    "stun.wifirst.net:3478",
    "stun.xtratelecom.es:3478",
    "stun.zadarma.com:3478",
    "stun1.faktortel.com.au:3478",
    "stun1.l.google.com:19302",
    "stun2.l.google.com:19302",
    "stun3.l.google.com:19302",
    "stun4.l.google.com:19302",
    "stun.nextcloud.com:443",
    "relay.webwormhole.io:3478"
	,nullptr};