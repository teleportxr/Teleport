// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <memory>

#include <common_p.hpp>
#include <node_p.hpp>
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

namespace avs
{
	struct GeometrySource::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(GeometrySource, Node)
		GeometrySourceBackendInterface *m_backend;
		GeometryRequesterBackendInterface *m_requesterBackend;
	};
	struct GeometryTarget::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(GeometryTarget, Node)
		GeometryTargetBackendInterface *m_backend;
	};
} // avs