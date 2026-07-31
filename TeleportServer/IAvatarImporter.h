#pragma once

#include "TeleportServer/IAvatarValidator.h"

namespace teleport
{
	namespace server
	{
		//! Pluggable importer for accepted avatars (Phase 4 of
		//! plans/avatars_implementation.md: import mode). Turning validated
		//! bytes into engine scene nodes is host-application territory —
		//! the TeleportServer library does NOT ship a default, on purpose,
		//! for the same reason as IAvatarValidator: it would drag a glTF
		//! parser and an asset pipeline into the protocol library. Hosts
		//! that don't supply an importer keep node_uid=0 in avatar-result.
		//!
		//! The node uid returned by an import is the session uid of the
		//! avatar root node in the server scene; it travels back to the
		//! owning client in avatar-result.node_uid and reaches peers as
		//! ordinary streamed geometry.
		class IAvatarImporter
		{
		public:
			virtual ~IAvatarImporter() = default;

			//! Import a validated avatar for a client. `validated` is the
			//! verdict the host's IAvatarValidator produced (contentHash /
			//! format identify the bytes). Returns the root node uid of the
			//! imported sub-tree, or 0 on failure.
			virtual avs::uid ImportValidatedForClient(avs::uid clientID, const AvatarValidationResult &validated) = 0;

			//! Import the server's default avatar for a client. Returns the
			//! node uid, or 0 when no default is configured.
			virtual avs::uid ImportDefaultForClient(avs::uid clientID) = 0;

			//! Remove a client's avatar node (disconnect, revoke, rejection).
			virtual void RemoveForClient(avs::uid clientID) = 0;
		};
	}
}
