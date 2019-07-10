// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include <libavstream/decoders/dec_interface.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include <map>

#include "App.h"

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class GeometryDecoder final : public avs::GeometryDecoderBackendInterface
{
public:
    GeometryDecoder();
    ~GeometryDecoder();

    // Inherited via GeometryDecoderBackendInterface
    virtual avs::Result decode(const void* buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface* target) override;

private:
    struct DecodedGeometry
    {
        std::map<avs::uid, size_t> primitiveArraysSizes;
        std::map<avs::uid, std::vector<avs::PrimitiveArray>> primitiveArrays;
        std::vector<avs::Accessor> accessors;
        std::vector<avs::BufferView> bufferViews;
        std::vector<avs::GeometryBuffer> buffers;

        size_t primitiveArraysMapSize;
        size_t accessorsSize;
        size_t bufferViewsSize;
        size_t buffersSize;

        ~DecodedGeometry()
        {
            primitiveArraysMapSize = 0;
            accessorsSize = 0;
            bufferViewsSize = 0;
            buffersSize = 0;

            accessors.clear();
            bufferViews.clear();
            buffers.clear();

            primitiveArraysSizes.clear();

            for (std::map<avs::uid, std::vector<avs::PrimitiveArray>>::iterator it = primitiveArrays.begin(); it != primitiveArrays.end(); it++)
                it->second.clear();

            primitiveArrays.clear();
        }
    };

    std::vector<DecodedGeometry> m_DecodedGeometries;
};