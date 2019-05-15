// (C) Copyright 2018 Simul.co

#pragma once

#include <libavstream/surfaces/surface_interface.hpp>
#include <SurfaceTexture.h>

class VideoSurface final : public avs::SurfaceBackendInterface
{
public:
    VideoSurface(OVR::SurfaceTexture* texture) : mTexture(texture) {}

    operator OVR::SurfaceTexture*() const { return mTexture; }
    OVR::SurfaceTexture* GetTexture() const { return mTexture; }

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
    OVR::SurfaceTexture* mTexture;
};

