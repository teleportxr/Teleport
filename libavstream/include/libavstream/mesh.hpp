// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

namespace avs
{
	/*!
	 * Mesh node `[passive, 1/1]`
	 *
	 * Provides access to geometry source data for other nodes in the pipeline.
	 */
	class AVSTREAM_API GeometrySource final : public Node
		, public GeometrySourceInterface
	{
		AVSTREAM_PUBLICINTERFACE(GeometrySource)
	public:
		GeometrySource();
		/*!
		 * Configure GeometrySource node.
		 * \param backend backend associated with this node.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if has already been configured with a backend.
		 *  - Result::Mesh_InvalidBackend if backend is nullptr.
		 */
		Result configure(GeometrySourceBackendInterface* sourceBackend, GeometryRequesterBackendInterface *req);

		GeometrySourceBackendInterface* getGeometrySourceBackendInterface() const override;

		GeometryRequesterBackendInterface* getGeometryRequesterBackendInterface() const override;

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
		const char* getDisplayName() const override { return "Geometry Source"; }
	};

	/*!
	 * Mesh node `[passive, 1/1]`
	 *
	 * Provides access to geometry source data for other nodes in the pipeline.
	 */
	class AVSTREAM_API GeometryTarget final : public Node
		, public GeometryTargetInterface
	{
		AVSTREAM_PUBLICINTERFACE(GeometryTarget)
	public:
		GeometryTarget();
		/*!
		 * Configure GeometrySource node.
		 * \param backend backend associated with this node.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if has already been configured with a backend.
		 *  - Result::Mesh_InvalidBackend if backend is nullptr.
		 */
		Result configure(GeometryTargetBackendInterface *);

		GeometryTargetBackendInterface* getGeometryTargetBackendInterface() const override;

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
		const char* getDisplayName() const override { return "Geometry Target"; }

	};
} // avs