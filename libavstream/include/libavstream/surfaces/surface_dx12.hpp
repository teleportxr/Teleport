// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

#include <d3d12.h>

namespace avs
{

	/*!
	 * Direct3D12 texture surface.
	 *
	 * \note Instance of this class holds reference on the underlying ID3D12Resource object.
	 */
	class AVSTREAM_API SurfaceDX12 final : public SurfaceBackendInterface
	{
	public:
		/*!
		 * Constructor.
		 * \param resource D3D12 texture resource.
		 */
		explicit SurfaceDX12(ID3D12Resource* resource = nullptr);
		~SurfaceDX12();

		/*! Set D3D12 texture resource as this surface resource handle. */
		void setResource(ID3D12Resource* resource);

		/* Begin SurfaceInterface */
		int getWidth() const override;
		int getHeight() const override;
		SurfaceFormat getFormat() const override;

		void* getResource() const override
		{
			return reinterpret_cast<void*>(m_resource);
		}
		/* End SurfaceInterface */

	private:
		ID3D12Resource* m_resource;
		D3D12_RESOURCE_DESC m_desc;
	};

} // avs
