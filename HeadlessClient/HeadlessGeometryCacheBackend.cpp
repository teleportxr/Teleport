#include "HeadlessGeometryCacheBackend.h"

std::vector<avs::uid> HeadlessGeometryCacheBackend::emptyVector;

const std::vector<avs::uid> &HeadlessGeometryCacheBackend::GetCompletedNodes() const
{
	return emptyVector;
}

std::vector<avs::uid> HeadlessGeometryCacheBackend::GetReceivedResources() const
{
	return std::vector<avs::uid>();
}

std::vector<avs::uid> HeadlessGeometryCacheBackend::GetResourceRequests() const
{
	return std::vector<avs::uid>();
}

void HeadlessGeometryCacheBackend::ClearCompletedNodes()
{
}

void HeadlessGeometryCacheBackend::ClearReceivedResources()
{
}

void HeadlessGeometryCacheBackend::ClearResourceRequests()
{
}

void HeadlessGeometryCacheBackend::ClearAll()
{
}
