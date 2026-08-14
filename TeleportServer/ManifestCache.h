#pragma once

#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

#include "TeleportServer/IAvatarManifestResolver.h"

namespace teleport
{
	namespace server
	{
		//! LRU cache of manifest evaluations, keyed on the resolved manifest
		//! url and bounded by both entry count and total bytes. Same shape and
		//! defaults as AvatarValidationCache.
		//!
		//! The entry lifetime is the manifest's own expiresAt, never the HTTP
		//! response's freshness. The UMID resolver serves
		//! `Cache-Control: public, max-age=60`, but its contract says
		//! consumers MUST enforce the manifest TTL regardless of HTTP caching,
		//! so HTTP freshness can only ever shorten what is stored here.
		class ManifestCache
		{
		public:
			explicit ManifestCache(size_t maxEntries = 256, size_t maxBytes = 8 * 1024 * 1024)
				: maxEntries(maxEntries ? maxEntries : 1)
				, maxBytes(maxBytes)
			{
			}

			//! Look up a still-valid evaluation. Returns false on a miss or
			//! when the stored manifest has expired, which is not the same
			//! thing: an expired entry is dropped rather than served, so a
			//! caller that re-fetches gets an honest fresh verdict.
			bool Get(const std::string &url, int64_t nowUnix, AvatarManifestResolution &out)
			{
				std::lock_guard<std::mutex> lock(mutex);
				auto it = index.find(url);
				if (it == index.end())
					return false;
				if (it->second->expiresAtUnix <= nowUnix)
				{
					Erase(it);
					return false;
				}
				// Promote to most-recently-used.
				entries.splice(entries.begin(), entries, it->second);
				out = it->second->resolution;
				return true;
			}

			void Set(const std::string &url, const AvatarManifestResolution &resolution, size_t bytes)
			{
				std::lock_guard<std::mutex> lock(mutex);
				auto it = index.find(url);
				if (it != index.end())
					Erase(it);

				entries.push_front(Entry{ url, resolution, resolution.expiresAtUnix, bytes });
				index[url] = entries.begin();
				totalBytes += bytes;

				while (entries.size() > maxEntries || totalBytes > maxBytes)
				{
					if (entries.empty())
						break;
					auto last = std::prev(entries.end());
					totalBytes -= last->bytes;
					index.erase(last->url);
					entries.pop_back();
				}
			}

			void Clear()
			{
				std::lock_guard<std::mutex> lock(mutex);
				entries.clear();
				index.clear();
				totalBytes = 0;
			}

			size_t Size() const
			{
				std::lock_guard<std::mutex> lock(mutex);
				return entries.size();
			}

			size_t Bytes() const
			{
				std::lock_guard<std::mutex> lock(mutex);
				return totalBytes;
			}

		private:
			struct Entry
			{
				std::string              url;
				AvatarManifestResolution resolution;
				int64_t                  expiresAtUnix = 0;
				size_t                   bytes = 0;
			};

			using EntryList = std::list<Entry>;
			using Index     = std::unordered_map<std::string, EntryList::iterator>;

			void Erase(Index::iterator it)
			{
				totalBytes -= it->second->bytes;
				entries.erase(it->second);
				index.erase(it);
			}

			mutable std::mutex mutex;
			EntryList          entries;	//!< front = most recently used
			Index              index;
			size_t             maxEntries;
			size_t             maxBytes;
			size_t             totalBytes = 0;
		};
	}
}
