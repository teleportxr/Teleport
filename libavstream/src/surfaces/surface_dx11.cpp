// libavstream
// (c) Copyright 2018-2024 Teleport XR Ltd

#include <libavstream/surfaces/surface_dx11.hpp>

namespace avs
{
	SurfaceDX11::SurfaceDX11(ID3D11Texture2D* resource)
		: m_resource(nullptr)
		, m_desc({})
	{
		setResource(resource);
	}

	SurfaceDX11::~SurfaceDX11()
	{
		if (m_resource)
		{
			m_resource->Release();
		}
	}

	void SurfaceDX11::setResource(ID3D11Texture2D* resource)
	{
		if (m_resource)
		{
			m_resource->Release();
		}
		m_resource = resource;
		if (m_resource)
		{
			m_resource->AddRef();
			m_resource->GetDesc(&m_desc);
		}
		else {
			m_desc = {};
		}
	}

	int SurfaceDX11::getWidth() const
	{
		return m_desc.Width;
	}

	int SurfaceDX11::getHeight() const
	{
		return m_desc.Height;
	}

	SurfaceFormat SurfaceDX11::getFormat() const
	{
		switch (m_desc.Format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return SurfaceFormat::ARGB;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return SurfaceFormat::ABGR;
		case DXGI_FORMAT_R10G10B10A2_UNORM:
			return SurfaceFormat::ARGB10;
		case DXGI_FORMAT_R16G16B16A16_UNORM:
			return SurfaceFormat::ARGB16;
		case DXGI_FORMAT_NV12:
			return SurfaceFormat::NV12;
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16_TYPELESS:
			return SurfaceFormat::R16;
		default:
			break;
		}
		return SurfaceFormat::Unknown;
	}

} // avs