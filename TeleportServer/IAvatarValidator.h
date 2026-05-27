#pragma once

#include <functional>
#include <string>
#include <vector>

#include "TeleportCore/Avatars.h"

namespace teleport
{
	namespace server
	{
		//! Verdict returned by an IAvatarValidator. Mirrors the
		//! avatar-result reasons enumerated in plans/avatars_plan.md §3.3 —
		//! `reasons` is a list of short machine-readable codes such as
		//! "file_too_large", "format_not_allowed", "hash_mismatch",
		//! "download_failed". The optional `message` is human-readable
		//! detail intended for client UX; do not parse it.
		struct AvatarValidationResult
		{
			bool                     ok = false;
			std::vector<std::string> reasons;
			std::string              message;
			//! Canonical content hash in the form "sha256:<hex>".
			std::string              contentHash;
			//! Size of the asset in bytes as measured by the validator
			//! (i.e. the on-wire length, not the declared length).
			std::uint64_t            bytes = 0;
			//! Canonical short format name as detected by the validator
			//! (e.g. "glb", "vrm"). Empty if the validator could not
			//! determine it.
			std::string              format;
		};

		//! Pluggable validator for incoming avatar offers. Host applications
		//! supply an implementation; the TeleportServer library does NOT
		//! ship a default, on purpose — V1–V7 in plans/avatars_plan.md §5
		//! mandate transport-level limits, SSRF guards and a parser, and
		//! pulling those dependencies into the protocol library would be a
		//! significant compromise. Hosts that don't supply a validator stay
		//! on the Phase-2 behaviour (always reply using_default).
		//!
		//! Implementations MUST be safe to call from arbitrary threads (the
		//! signaling layer dispatches offers as they arrive). The callback
		//! supplied to `Validate` is invoked with the verdict; it MAY be
		//! invoked synchronously from inside `Validate` (e.g. on a cache
		//! hit) or asynchronously from a worker thread.
		class IAvatarValidator
		{
		public:
			using Callback = std::function<void(const AvatarValidationResult &)>;

			virtual ~IAvatarValidator() = default;

			//! Validate the `offer` against the supplied `requirements`
			//! and invoke `cb` with the verdict. `requirements` is the
			//! same struct that travelled in the originating avatar-policy.
			virtual void Validate(
				const core::AvatarOffer        &offer,
				const core::AvatarRequirements &requirements,
				Callback                        cb) = 0;
		};
	}
}
