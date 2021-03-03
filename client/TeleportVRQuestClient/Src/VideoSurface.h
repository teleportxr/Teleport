// (C) Copyright 2018 Simul.co

#pragma once

#include <libavstream/surfaces/surface_interface.hpp>
#include <Render/SurfaceTexture.h>

class VideoSurface final : public avs::SurfaceBackendInterface
{
public:
    VideoSurface(OVRFW::SurfaceTexture* texture) : mTexture(texture) {}

    operator OVRFW::SurfaceTexture*() const { return mTexture; }
	OVRFW::SurfaceTexture* GetTexture() const { return mTexture; }

    /* Begin avs::SurfaceBackendInterface */
    int getWidth() const override
    {
        return 0;
    }
    int getHeight() const override
    {
        return 0;
    }
    avs::SurfaceFormat getFormat() const override
    {
        return avs::SurfaceFormat::Unknown;
    }
    void* getResource() const override
    {
        return reinterpret_cast<void*>(mTexture);
    }
    /* End avs::SurfaceBackendInterface */

private:
	OVRFW::SurfaceTexture* mTexture;
};

