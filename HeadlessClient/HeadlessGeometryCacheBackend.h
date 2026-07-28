#pragma once

#include "TeleportClient/GeometryCacheBackendInterface.h"

class HeadlessGeometryCacheBackend : public teleport::client::GeometryCacheBackendInterface
{
public:
	HeadlessGeometryCacheBackend() = default;
	virtual ~HeadlessGeometryCacheBackend() = default;

	const std::vector<avs::uid> &GetCompletedNodes() const override;
	std::vector<avs::uid> GetReceivedResources() const override;
	std::vector<avs::uid> GetResourceRequests() const override;
	void ClearCompletedNodes() override;
	void ClearReceivedResources() override;
	void ClearResourceRequests() override;
	void ClearAll() override;

private:
	static std::vector<avs::uid> emptyVector;
};
