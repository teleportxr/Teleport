// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

namespace avs {

/*!
 * Surface node `[passive, 1/1]`
 *
 * Provides access to surface for other nodes in the pipeline.
 */
class AVSTREAM_API Surface final : public PipelineNode
	                             , public SurfaceInterface
{
	AVSTREAM_PUBLICINTERFACE(Surface)
public:
	Surface();

	/*!
	 * Configure surface node.
	 * \param surfaceBackend Surface backend associated with this node.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_AlreadyConfigured if surface has already been configured with a backend.
	 *  - Result::Surface_InvalidBackend if surfaceBackend is nullptr.
	 *
	 * Surface node takes ownership of its backend.
	 */
	Result configure(SurfaceBackendInterface* surfaceBackend, SurfaceBackendInterface* alphaSurfaceBackend = nullptr);

	/*!
	 * Deconfigure surface node and release its backend.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::Node_NotConfigured if surface has not been configured.
	 */
	Result deconfigure() override;
	
	/*!
	 * Get node display name (for reporting & profiling).
	 */
	const char* getDisplayName() const override { return "Surface"; }

	/*! Return a non-owning pointer to associated surface backend. */
	SurfaceBackendInterface* getBackendSurface() const override;

	/*! Return a non-owning pointer to associated surface backend. */
	SurfaceBackendInterface* getAlphaBackendSurface() const override;
};

} // avs