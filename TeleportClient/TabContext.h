#pragma once
#include <libavstream/common.hpp>
#include "basic_linear_algebra.h"
#include <set>

namespace teleport
{
	namespace client
	{
		//! A "tab" in the Teleport client represents an ongoing connection to a single server
		//! The tab manages sessionClient creation/destruction.
		//! The tab may retain a temporary connection to an outgoing server while a new connection is established.
		class TabContext
		{
			avs::uid server_uid=0;
			avs::uid next_server_uid=0;
		public:
			TabContext();
			static const std::set<int32_t> &GetTabIndices();
			static std::shared_ptr<TabContext> GetTabContext(int32_t index);
			static int32_t GetEmptyTabContext();
			static int32_t AddTabContext();
			static void DeleteTabContext(int32_t index);
			static void ConnectButtonHandler(int32_t tab_context_id, const std::string &url);
			static void CancelConnectButtonHandler(int32_t tab_context_id);
			static bool ShouldFollowExternalURL();
			static std::string PopExternalURL();

			void ConnectTo(std::string url);
			void CancelConnection();
			avs::uid GetServerUid() const
			{
				return server_uid;
			}
			avs::uid GetNextServerUid() const
			{
				return next_server_uid;
			}

			//! Communication from SessionClient
			void ConnectionComplete(avs::uid uid);

			//! Is the tabContext connected or connecting?
			//! If not, it's available for a connection.
			bool IsInUse() const;
		};
	}
}