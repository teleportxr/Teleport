// Copyright 2018-2024 Teleport XR Ltd

#pragma once
#include "ServerSettings.h"
#include "Export.h"
typedef void(TELEPORT_STDCALL *ProcessAudioInputFn)(avs::uid uid, const uint8_t *data, size_t dataSize);
typedef void(TELEPORT_STDCALL *OutputLogFn)(int severity,const char *txt);
typedef bool(TELEPORT_STDCALL *ClientStoppedRenderingNodeFn)(avs::uid clientID, avs::uid nodeID);
typedef bool(TELEPORT_STDCALL *ClientStartedRenderingNodeFn)(avs::uid clientID, avs::uid nodeID);
typedef void(TELEPORT_STDCALL* SetHeadPoseFn) (avs::uid client_uid, const teleport::core::Pose*);
typedef void(TELEPORT_STDCALL* SetControllerPoseFn) (avs::uid uid, int index, const teleport::core::PoseDynamic*);
typedef void(TELEPORT_STDCALL *ProcessNewInputStateFn)(avs::uid client_uid, const teleport::core::InputState *, const uint8_t **, const float **);
typedef void(TELEPORT_STDCALL *ProcessNewInputEventsFn)(avs::uid client_uid, uint16_t, uint16_t, uint16_t, const teleport::core::InputEventBinary **, const teleport::core::InputEventAnalogue **, const teleport::core::InputEventMotion **);
typedef void(TELEPORT_STDCALL *DisconnectFn)(avs::uid client_uid);
typedef void(TELEPORT_STDCALL *ReportHandshakeFn)(avs::uid client_uid, const teleport::core::Handshake *h);
typedef int64_t(* GetUnixTimestampFn)();	// was __stdcall*
		
namespace avs
{
	/*!
	 * Message handler function prototype.
	 *
	 * \param severity Message severity class.
	 * \param msg Null-terminated string containing the message.
	 * \param userData Custom user data pointer.
	 */
	typedef void(*MessageHandlerFunc)(LogSeverity severity, const char* msg, void* userData);
}

namespace teleport
{
	namespace server
	{
		extern void TELEPORT_SERVER_API ConvertTransform(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::Transform &transform);
		extern void TELEPORT_SERVER_API ConvertRotation(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec4 &rotation);
		extern void TELEPORT_SERVER_API ConvertPosition(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec3 &position);
		extern void TELEPORT_SERVER_API ConvertScale(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, vec3 &scale);
		extern int8_t TELEPORT_SERVER_API ConvertAxis(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, int8_t axis);
		/// The collected values required to initialize a server session; see Server_Teleport_Initialize().
		struct InitializationSettings
		{
			const char *clientIP;			   ///< IP address to match to connecting clients. May be blank.
			const char *httpMountDirectory;  ///< Local (server-side) directory for HTTP requests: usually the Teleport cache directory.
			const char *certDirectory;	   ///< Local directory for HTTP certificates.
			const char *privateKeyDirectory; ///< Local directory for private keys.
			const char *signalingPorts;	   ///< Optional list of ports to listen for signaling connections and queries.

			ClientStoppedRenderingNodeFn clientStoppedRenderingNode; ///< Delegate to be called when client is no longer rendering a specified node.
			ClientStartedRenderingNodeFn clientStartedRenderingNode;
			SetHeadPoseFn headPoseSetter;
			SetControllerPoseFn controllerPoseSetter;
			ProcessNewInputStateFn newInputStateProcessing;
			ProcessNewInputEventsFn newInputEventsProcessing;
			DisconnectFn disconnect;
			avs::MessageHandlerFunc messageHandler;
			ReportHandshakeFn reportHandshake;
			ProcessAudioInputFn processAudioInput;
			GetUnixTimestampFn getUnixTimestampNs;
			int64_t start_unix_time_us;
		};
		extern bool TELEPORT_SERVER_API ApplyInitializationSettings(const InitializationSettings *initializationSettings);
		extern void TELEPORT_SERVER_API SetOutputLogCallback(OutputLogFn fn);
		extern ServerSettings TELEPORT_SERVER_API &GetServerSettings();
	}
}