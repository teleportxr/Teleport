// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#include <libavstream/surfaces/surface_dx12.hpp>

namespace avs {

	SurfaceDX12::SurfaceDX12(ID3D12Resource* resource)
		: m_resource(nullptr)
		, m_desc({})
	{
		setResource(resource);
	}

	SurfaceDX12::~SurfaceDX12()
	{
		if (m_resource)
		{
			m_resource->Release();
		}
	}

	void SurfaceDX12::setResource(ID3D12Resource* resource)
	{
		if (m_resource)
		{
			m_resource->Release();
		}
		m_resource = resource;
		if (m_resource)
		{
			m_resource->AddRef();
			m_desc = m_resource->GetDesc();
		}
		else {
			m_desc = {};
		}
	}

	int SurfaceDX12::getWidth() const
	{
		return (int)m_desc.Width;
	}

	int SurfaceDX12::getHeight() const
	{
		return m_desc.Height;
	}

	SurfaceFormat SurfaceDX12::getFormat() const
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