// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

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
	class AVSTREAM_API GeometrySource final : public PipelineNode
		, public GeometrySourceInterface
	{
		AVSTREAM_PUBLICINTERFACE(GeometrySource)
	public:
		GeometrySource();
		/// Configure GeometrySource node.
		/// \param req What requests the geometry.
		/// \return
		///  - Result::OK on success.
		///  - Result::Node_AlreadyConfigured if has already been configured with a backend.
		///  - Result::Mesh_InvalidBackend if backend is nullptr.
		Result configure( GeometryRequesterBackendInterface *req);

		GeometryRequesterBackendInterface* getGeometryRequesterBackendInterface() const override;

		/// 
		/// Deconfigure surface node and release its backend.
		/// \return
		///  - Result::OK on success.
		///  - Result::Node_NotConfigured if surface has not been configured.
		/// 
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
	class AVSTREAM_API GeometryTarget final : public PipelineNode
		, public GeometryTargetInterface
	{
		AVSTREAM_PUBLICINTERFACE(GeometryTarget)
	public:
		GeometryTarget();
		/*!
		 * Configure GeometrySource node.
		 * \param target backend associated with this target node.
		 * \return
		 *  - Result::OK on success.
		 *  - Result::Node_AlreadyConfigured if has already been configured with a backend.
		 *  - Result::Mesh_InvalidBackend if backend is nullptr.
		 */
		Result configure(GeometryTargetBackendInterface *target);

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