// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/surfaces/surface_interface.hpp>

#include <d3d11.h>

namespace avs
{

/*!
 * Direct3D11 texture surface.
 *
 * Instance of this class holds reference on the underlying ID3D11Texture2D object.
 */
class AVSTREAM_API SurfaceDX11 final : public SurfaceBackendInterface
{
public:
	/*!
	 * Constructor.
	 * \param resource D3D11 texture resource.
	 */
	explicit SurfaceDX11(ID3D11Texture2D* resource=nullptr);
	~SurfaceDX11();

	/*! Set D3D11 texture resource as this surface resource handle. */
	void setResource(ID3D11Texture2D* resource);
	
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
	ID3D11Texture2D* m_resource;
	D3D11_TEXTURE2D_DESC m_desc;
};

} // avs
