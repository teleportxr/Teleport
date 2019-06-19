#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>


/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.
*/
class GeometryDecoder: public avs::GeometryDecoderBackendInterface
{
public:
	GeometryDecoder();
	~GeometryDecoder();
	// Inherited via GeometryDecoderBackendInterface
	virtual avs::Result decode(const void * buffer, size_t bufferSizeInBytes, avs::GeometryPayloadType type, avs::GeometryTargetBackendInterface * target) override;

};

