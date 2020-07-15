// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/memory.hpp>

namespace avs
{

/*! Surface format. */
enum class SurfaceFormat
{
	Unknown = 0, /*!< Unknown or invalid format. */
	ARGB,        /*!< 8-bits per channel ARGB format. */
	ABGR,        /*!< 8-bits per channel ABGR format. */
	NV12,        /*!< NV12 format (YCrCb color encoding). */
	R16,         /*!< 16-bits single channel format. */
	ARGB10,      /*!< 10-bits per color channels, 2 bits for alpha. ARGB10 format. */
	ABGR10,      /*!< 10-bits per color channels, 2 bits for alpha. ABGR10 format. */
	ARGB16,      /*!< 16-bits per channel ARGB16 format. */
	ABGR16       /*!< 16-bits per channel ABGR16 format. */
};

/*!
 * Common surface backend interface.
 *
 * Surface backend abstracts graphics API specific hardware-accelerated surface handle.
 */
class AVSTREAM_API SurfaceBackendInterface : public UseInternalAllocator
{
public:
	virtual ~SurfaceBackendInterface() = default;

	/*! Get surface width in pixels. */
	virtual int getWidth() const = 0;
	/*! Get surface height in pixels. */
	virtual int getHeight() const = 0;
	/*! Get surface format. */
	virtual SurfaceFormat getFormat() const = 0;

	/*! 
	 * Get native surface handle (graphics API specific).
	 *
	 * \note Can return nullptr for surfaces that do not support querying for handle).
	 */
	virtual void* getResource() const = 0;
};

} // avs
