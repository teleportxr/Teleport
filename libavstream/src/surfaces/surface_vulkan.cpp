// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#if LIBAVSTREAM_SUPPORT_VULKAN
#include <libavstream/surfaces/surface_vulkan.hpp>

namespace avs {

	SurfaceVulkan::SurfaceVulkan(vk::Image* resource,int w,int h,vk::Format f)
		: m_resource(nullptr)
	{
		setResource(resource,w,h,f);
	}

	SurfaceVulkan::~SurfaceVulkan()
	{
	}

	void SurfaceVulkan::setResource(vk::Image* resource,int w,int h,vk::Format f)
	{
		m_resource = resource;
		width=w;
		height=h;
		format=f;
	}

	int SurfaceVulkan::getWidth() const
	{
		return (int)width;
	}

	int SurfaceVulkan::getHeight() const
	{
		return height;
	}

	SurfaceFormat SurfaceVulkan::getFormat() const
	{
		switch (format)
		{
		case vk::Format::eR8G8B8A8Unorm:
			return SurfaceFormat::ARGB;
		case vk::Format::eB8G8R8A8Unorm:
			return SurfaceFormat::ABGR;
		case vk::Format::eA2R10G10B10UnormPack32:
			return SurfaceFormat::ARGB10;
		case vk::Format::eR16G16B16A16Unorm:
			return SurfaceFormat::ARGB16;
		case vk::Format::eR16Sfloat:
		case vk::Format::eR16Unorm:
			return SurfaceFormat::R16;
		default:
			break;
		}
		return SurfaceFormat::Unknown;
	}

} // avs

#endif