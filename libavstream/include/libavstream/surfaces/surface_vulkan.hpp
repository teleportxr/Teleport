// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/surfaces/surface_interface.hpp>
#if LIBAVSTREAM_SUPPORT_VULKAN
#include <vulkan/vulkan.hpp>

namespace avs
{

	/*!
	 * Vulkan texture surface.
	 *
	 * \note Instance of this class holds reference on the underlying Vulkan image object.
	 */
	class AVSTREAM_API SurfaceVulkan final : public SurfaceBackendInterface
	{
	public:
		/*!
		 * Constructor.
		 * \param resource Vulkan texture resource.
		 * \param w Width in pixels.
		 * \param h Height in pixels.
		 * \param f Graphic format.
		 */
		explicit SurfaceVulkan(vk::Image* resource = nullptr,int w=0,int h=0,vk::Format f=vk::Format::eUndefined);
		~SurfaceVulkan();

		/*! Set D3D12 texture resource as this surface resource handle. */
		void setResource(vk::Image* resource,int width,int height,vk::Format format);

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
		vk::Image* m_resource;
		int width=0,height=0;
		vk::Format format=vk::Format::eUndefined;
	};

} // avs

#endif