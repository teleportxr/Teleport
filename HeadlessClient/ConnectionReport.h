#pragma once

#include <libavstream/common.hpp>
#include <cstdint>
#include <string>
#include <vector>

//! Plain data describing one connection's state and the geometry streamed to it.
//!
//! These structs are the single source of truth behind both renderings of a control
//! response: the prose a terminal sees, and the JSON a machine client sees (see
//! docs/protocol/local_control.rst). The producers - HeadlessConnection and
//! HeadlessGeometryCacheBackend - fill them under their own locks, and the Render*
//! functions below turn them back into the exact text the protocol has always
//! emitted. Status and geometry prose is assembled nowhere else: if the two
//! renderings are ever to agree, they must come from one struct.

//! One connection's state, as reported by `status` and summarised by `connections`.
struct ConnectionStatus
{
	//! False when no SessionClient exists yet, in which case every field below is unset.
	bool hasSession = false;
	//! Name of the teleport::client::ConnectionStatus enumerant - UNCONNECTED, OFFERING,
	//! AWAITING_SETUP, HANDSHAKING, CONNECTED, RECONNECTING or UNKNOWN - or DISCONNECTED
	//! when there is no session at all.
	std::string state = "DISCONNECTED";
	//! Parenthesised note following the state, e.g. "no session". Usually empty.
	std::string detail;
	std::string server;
	int port = 0;
	int latencyMs = 0;
	size_t inputsAvailable = 0;
};

//! Totals from the geometry cache, as reported by bare `geometry`.
struct GeometryCounts
{
	size_t nodes			   = 0;
	size_t nodesRemoved		   = 0;
	size_t skeletons		   = 0;
	size_t resourcesReceived   = 0;
	size_t pointers			   = 0;
	size_t referencedUnsent	   = 0;
	size_t pendingResourceAcks = 0;
	size_t pendingNodeAcks	   = 0;
};

//! A payload type acknowledged without being parsed, and how many of them.
struct UnparsedPayloadCount
{
	std::string type;
	size_t		count = 0;
};

//! One tracked node, as reported by `geometry nodes`.
struct GeometryNodeEntry
{
	avs::uid	uid		 = 0;
	std::string name;
	int			dataType = 0;
	avs::uid	data	 = 0;
	avs::uid	parent	 = 0;
	avs::uid	skeleton = 0;
	size_t		materials  = 0;
	size_t		animations = 0;
	std::string url; //!< Set for Link nodes.
};

//! One resource the server gave us a URL for rather than inline data.
struct GeometryPointerEntry
{
	avs::uid	uid = 0;
	std::string type; //!< avs::stringOf(GeometryPayloadType).
	std::string url;
};

//! Everything the geometry cache knows, gathered under one lock. The `what` argument
//! of the `geometry` command selects which parts get rendered, not which get gathered.
struct GeometryReport
{
	//! False when the connection has no geometry cache at all.
	bool							  hasCache = false;
	GeometryCounts					  counts;
	std::vector<UnparsedPayloadCount> unparsed;
	std::vector<GeometryNodeEntry>	  nodes;
	std::vector<GeometryPointerEntry> pointers;
	std::vector<avs::uid>			  referencedUnsent;
};

//! Full `status` body: state, server, latency and input count.
std::string RenderStatus(const ConnectionStatus &status);

//! Just the "Status: ..." line, as `connections` shows for each entry.
std::string RenderStatusLine(const ConnectionStatus &status);

std::string RenderGeometrySummary(const GeometryReport &report);
std::string RenderGeometryNodes(const GeometryReport &report);
std::string RenderGeometryResources(const GeometryReport &report);
