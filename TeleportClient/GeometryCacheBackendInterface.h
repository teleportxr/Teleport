#pragma once
#include <libavstream/common.hpp>
#include <vector>

namespace teleport
{
	namespace client
	{
		class GeometryCacheBackendInterface
		{
		public:
			virtual const std::vector<avs::uid> &GetCompletedNodes() const = 0;
			virtual std::vector<avs::uid> GetReceivedResources() const = 0;
			virtual std::vector<avs::uid> GetResourceRequests() const = 0;
			virtual void ClearCompletedNodes() = 0;
			virtual void ClearReceivedResources() = 0;
			virtual void ClearResourceRequests() = 0;
		};
	}
}