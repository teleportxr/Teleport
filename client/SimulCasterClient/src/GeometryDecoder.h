#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include <map>

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class GeometryDecoder final : public avs::GeometryDecoderBackendInterface
{
public:
    GeometryDecoder();
    ~GeometryDecoder();
    // Inherited via GeometryDecoderBackendInterface
    virtual avs::Result decode(const void * buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface * target) override;

private:
    struct DecodedGeometry
    {
        std::map<avs::uid, std::vector<avs::PrimitiveArray>> primitiveArrays;
        std::map<avs::uid, avs::Accessor> accessors;
        std::map<avs::uid, avs::BufferView> bufferViews;
        std::map<avs::uid, avs::GeometryBuffer> buffers;
        std::map<avs::uid, std::vector<uint8_t>> bufferDatas;

        ~DecodedGeometry()
        {
            for (auto& primitiveArray : primitiveArrays)
                primitiveArray.second.clear();
            primitiveArrays.clear();

            accessors.clear();
            bufferViews.clear();
            buffers.clear();

            for (auto& bufferData : bufferDatas)
                bufferData.second.clear();
            bufferDatas.clear();
        }
    };
};
