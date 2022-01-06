// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include <mesh_p.hpp>

using namespace avs;

uid avs::GenerateUid()
{
	static avs::uid static_uid = 1;
	if (!static_uid)
		throw(std::runtime_error("Out of uid's"));
	return static_uid++;
}

GeometrySource::GeometrySource()
	: PipelineNode(new GeometrySource::Private(this))
{}

GeometrySourceBackendInterface* GeometrySource::getGeometrySourceBackendInterface() const
{
	return d().m_backend;
}

GeometryRequesterBackendInterface* GeometrySource::getGeometryRequesterBackendInterface() const
{
	return d().m_requesterBackend;
}

Result GeometrySource::configure(GeometrySourceBackendInterface* sourceBackend,GeometryRequesterBackendInterface *req)
{
	if (d().m_requesterBackend)
	{
		Result deconf_res = deconfigure();
		if (deconf_res != Result::OK)
			return Result::Node_AlreadyConfigured;
	}
	if (!sourceBackend)
	{
		return Result::Surface_InvalidBackend;
	}
	d().m_backend= sourceBackend;
	d().m_requesterBackend = req;
	setNumSlots(1, 1);
	return Result::OK;
}

Result GeometrySource::deconfigure()
{
	if (!d().m_backend&&!d().m_requesterBackend)
	{
		return Result::Node_NotConfigured;
	}

	d().m_backend = nullptr;
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
