// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#pragma once

#include <memory>

#include "common_p.hpp"
#include "node_p.hpp"
#include <libavstream/surface.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

namespace avs
{

struct Surface::Private final : public PipelineNode::Private
{
	AVSTREAM_PRIVATEINTERFACE(Surface, PipelineNode)
	std::unique_ptr<SurfaceBackendInterface> m_backend;
	std::unique_ptr<SurfaceBackendInterface> m_alphaBackend;
};

} // avs