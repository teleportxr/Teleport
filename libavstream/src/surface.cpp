// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include "surface_p.hpp"

namespace avs {

	Surface::Surface()
		: PipelineNode(new Surface::Private(this))
	{}

	SurfaceBackendInterface* Surface::getBackendSurface() const
	{
		return d().m_backend.get();
	}

	SurfaceBackendInterface* Surface::getAlphaBackendSurface() const
	{
		return d().m_alphaBackend.get();
	}

	Result Surface::configure(SurfaceBackendInterface* surfaceBackend, SurfaceBackendInterface* alphaSurfaceBackend)
	{
		if (d().m_backend)
		{
			Result deconf_res=deconfigure();
			if(deconf_res!=Result::OK)
				return Result::Node_AlreadyConfigured;
		}

		if (!surfaceBackend)
		{
			return Result::Surface_InvalidBackend;
		}

		d().m_backend.reset(surfaceBackend);

		d().m_alphaBackend.reset(alphaSurfaceBackend);

		setNumSlots(1, 1);
		return Result::OK;
	}

	Result Surface::deconfigure()
	{
		if (!d().m_backend)
		{
			return Result::Node_NotConfigured;
		}

		d().m_backend.reset();

		d().m_alphaBackend.reset();

		setNumSlots(0, 0);
		return Result::OK;
	}

} // avs