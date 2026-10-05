// libavstream
// (c) Copyright 2018-2026 Teleport XR Ltd

#pragma once

#include <cstdint>
#include <string>

namespace avs
{
	//! Failure codes shared with the server's "error" signalling message (see
	//! docs/protocol/signaling.rst). Each names what went wrong, not which side noticed.
	namespace connection_failure
	{
		//! The server's ICE policy needs a TURN relay and no relay allocation succeeded.
		constexpr const char *kServerRelayUnavailable = "server-relay-unavailable";
		//! The server offered no usable ICE candidates.
		constexpr const char *kNoServerCandidates = "no-server-candidates";
		//! This client could not learn a public address: its network probably blocks UDP.
		constexpr const char *kClientUdpBlocked = "client-udp-blocked";
		//! Candidates were exchanged but no pair succeeded.
		constexpr const char *kIceTimeout = "ice-timeout";
	}

	enum class IceCandidateKind : uint8_t
	{
		Unknown,
		Host,
		ServerReflexive,
		PeerReflexive,
		Relayed
	};

	struct IceCandidateCounts
	{
		uint32_t host  = 0;
		uint32_t srflx = 0;
		uint32_t prflx = 0;
		uint32_t relay = 0;
		void Add(IceCandidateKind k)
		{
			switch (k)
			{
			case IceCandidateKind::Host:			host++; break;
			case IceCandidateKind::ServerReflexive:	srflx++; break;
			case IceCandidateKind::PeerReflexive:	prflx++; break;
			case IceCandidateKind::Relayed:			relay++; break;
			default: break;
			}
		}
		uint32_t Total() const
		{
			return host + srflx + prflx + relay;
		}
		std::string ToString() const
		{
			return "host=" + std::to_string(host) + " srflx=" + std::to_string(srflx) + " prflx=" + std::to_string(prflx)
				   + " relay=" + std::to_string(relay);
		}
	};

	//! Why the most recent connection attempt failed. Empty code means no failure is known.
	struct ConnectionFailure
	{
		std::string code;
		//! For logs and diagnostics panels; never shown verbatim as the headline message.
		std::string detail;
		//! True if retrying will not help until something changes on the server.
		bool fatal = false;
		//! True if the server reported this failure, false if the client inferred it.
		bool fromServer = false;
		bool Empty() const
		{
			return code.empty();
		}
	};

	//! What a network source knows about its current (or most recent) ICE session.
	struct ConnectionDiagnostics
	{
		IceCandidateCounts local;
		IceCandidateCounts remote;
		bool			   gatheringComplete = false;
		//! The server supplied a TURN server that this client cannot use (UDP mux mode).
		bool			   turnIgnoredByUdpMux = false;
		//! Survives reconnection attempts; cleared when a connection succeeds or a new
		//! connection is requested.
		ConnectionFailure  failure;
		//! Consecutive failed ICE sessions since the last success.
		uint32_t		   failedAttempts = 0;
	};

	//! Infer why ICE failed from what this client saw. A failure the server reported is
	//! authoritative and is returned unchanged.
	inline ConnectionFailure ClassifyConnectionFailure(const ConnectionDiagnostics &d)
	{
		if (!d.failure.Empty() && d.failure.fromServer)
			return d.failure;
		ConnectionFailure f;
		const std::string counts = "local " + d.local.ToString() + "; remote " + d.remote.ToString();
		if (d.remote.Total() == 0)
		{
			f.code	 = connection_failure::kNoServerCandidates;
			f.detail = "the server sent no ICE candidates (" + counts + ")";
		}
		else if (d.local.srflx == 0 && d.local.relay == 0)
		{
			f.code	 = connection_failure::kClientUdpBlocked;
			f.detail = "no public address could be discovered for this client (" + counts + ")";
		}
		else
		{
			f.code	 = connection_failure::kIceTimeout;
			f.detail = "no ICE candidate pair succeeded (" + counts + ")";
		}
		return f;
	}

	//! The one line to show a user for a failure code.
	inline const char *DescribeConnectionFailure(const std::string &code)
	{
		if (code == connection_failure::kServerRelayUnavailable)
			return "The server's media relay is unreachable. This is a server problem; retrying won't help until it is fixed.";
		if (code == connection_failure::kNoServerCandidates)
			return "The server offered no network route.";
		if (code == connection_failure::kClientUdpBlocked)
			return "Your network appears to block UDP. Try another network.";
		if (code == connection_failure::kIceTimeout)
			return "Could not establish a media connection with the server.";
		if (code.empty())
			return "";
		return "The connection to the server failed.";
	}
}
