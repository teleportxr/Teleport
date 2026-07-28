#include "HeadlessGeometryTarget.h"
#include "TeleportCore/Logging.h"

HeadlessGeometryTarget::HeadlessGeometryTarget()
{
	TELEPORT_LOG("HeadlessGeometryTarget created");
}

HeadlessGeometryTarget::~HeadlessGeometryTarget()
{
}

void HeadlessGeometryTarget::LogGeometryEvent(const std::string &event)
{
	TELEPORT_LOG("Geometry: {}", event);
}
