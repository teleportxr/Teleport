// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#include "mesh_p.hpp"

using namespace avs;

static avs::uid static_uid = 1;
uid avs::GenerateUid()
{
	if (!static_uid)
		throw(std::runtime_error("Out of uid's"));
	return static_uid++;
}

void avs::ClaimUidRange(avs::uid last)
{
	if (static_uid<=last)
		static_uid=last+1;
}

GeometrySource::GeometrySource()
	: PipelineNode(new GeometrySource::Private(this))
{}


GeometryRequesterBackendInterface* GeometrySource::getGeometryRequesterBackendInterface() const
{
	return d().m_requesterBackend;
}

Result GeometrySource::configure(GeometryRequesterBackendInterface *req)
{
	if (d().m_requesterBackend)
	{
		Result deconf_res = deconfigure();
		if (deconf_res != Result::OK)
			return Result::Node_AlreadyConfigured;
	}
	name="Geometry Source";
	d().m_requesterBackend = req;
	setNumSlots(1, 1);
	return Result::OK;
}

Result GeometrySource::deconfigure()
{
	if (!d().m_requesterBackend)
	{
		return Result::Node_NotConfigured;
	}

	d().m_requesterBackend = nullptr;
	setNumSlots(0, 0);
	return Result::OK;
}


GeometryTarget::GeometryTarget()
	: PipelineNode(new GeometryTarget::Private(this))
{}

GeometryTargetBackendInterface* GeometryTarget::getGeometryTargetBackendInterface() const
{
	return d().m_backend;
}

Result GeometryTarget::configure(GeometryTargetBackendInterface* surfaceBackend)
{
	if (d().m_backend)
	{
		Result deconf_res = deconfigure();
		if (deconf_res != Result::OK)
			return Result::Node_AlreadyConfigured;
	}
	if (!surfaceBackend)
	{
		return Result::Surface_InvalidBackend;
	}
	d().m_backend = surfaceBackend;
	setNumSlots(1, 1);
	return Result::OK;
}

Result GeometryTarget::deconfigure()
{
	if (!d().m_backend)
	{
		return Result::Node_NotConfigured;
	}

	d().m_backend = nullptr;
	setNumSlots(0, 0);
	return Result::OK;
}
