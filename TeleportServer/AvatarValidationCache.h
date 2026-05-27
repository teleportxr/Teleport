#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "TeleportServer/IAvatarValidator.h"

namespace teleport
{
	namespace server
	{
		//! Bounded LRU cache of past validation results keyed by content
		//! hash, used by host-supplied IAvatarValidator implementations to
		//! short-circuit re-validation of an asset two clients have already
		//! offered. Capacity is bounded both by entry count and by total
		//! cached bytes (plan §4.1: default 256 entries / 256 MiB).
		//!
		//! Thread-safe: a single internal mutex guards all mutations and
		//! reads, since every call either mutates the LRU recency order or
		//! evicts entries. Lookups are O(1) average; eviction is O(1).
		class AvatarValidationCache
		{
		public:
			static constexpr std::size_t kDefaultMaxEntries = 256;
			static constexpr std::uint64_t kDefaultMaxBytes = 256ull * 1024 * 1024;

			AvatarValidationCache(std::size_t maxEntries = kDefaultMaxEntries,
				std::uint64_t maxBytes = kDefaultMaxBytes)
				: maxEntries(maxEntries < 1 ? 1 : maxEntries)
				, maxBytes(maxBytes)
				, totalBytes(0)
			{
			}

			//! Returns the cached verdict for `contentHash` if present,
			//! promoting it to most-recently-used. Returns std::nullopt
			//! when no entry exists.
			std::optional<AvatarValidationResult> Get(const std::string &contentHash)
			{
				std::lock_guard<std::mutex> lk(mu);
				auto it = index.find(contentHash);
				if (it == index.end())
					return std::nullopt;
				// Promote.
				order.splice(order.begin(), order, it->second);
				return it->second->second;
			}

			//! Inserts or replaces `result` under `contentHash`, evicting
			//! older entries until both `maxEntries` and `maxBytes` are
			//! satisfied. `bytes` is the on-wire size of the cached asset.
			void Put(const std::string &contentHash, const AvatarValidationResult &result, std::uint64_t bytes)
			{
				std::lock_guard<std::mutex> lk(mu);
				auto it = index.find(contentHash);
				if (it != index.end())
				{
					totalBytes -= it->second->second.bytes;
					order.erase(it->second);
					index.erase(it);
				}
				AvatarValidationResult stored = result;
				stored.contentHash = contentHash;
				stored.bytes       = bytes;
				order.emplace_front(contentHash, std::move(stored));
				index[contentHash] = order.begin();
				totalBytes += bytes;
				EvictLocked();
			}

			std::size_t Size() const
			{
				std::lock_guard<std::mutex> lk(mu);
				return order.size();
			}

			std::uint64_t TotalBytes() const
			{
				std::lock_guard<std::mutex> lk(mu);
				return totalBytes;
			}

			void Clear()
			{
				std::lock_guard<std::mutex> lk(mu);
				order.clear();
				index.clear();
				totalBytes = 0;
			}

		private:
			using Entry  = std::pair<std::string, AvatarValidationResult>;
			using ListIt = std::list<Entry>::iterator;

			void EvictLocked()
			{
				while (order.size() > maxEntries || totalBytes > maxBytes)
				{
					if (order.empty()) break;
					auto &last = order.back();
					totalBytes -= last.second.bytes;
					index.erase(last.first);
					order.pop_back();
				}
			}

			mutable std::mutex                              mu;
			std::size_t                                     maxEntries;
			std::uint64_t                                   maxBytes;
			std::uint64_t                                   totalBytes;
			std::list<Entry>                                order;
			std::unordered_map<std::string, ListIt>         index;
		};
	}
}
