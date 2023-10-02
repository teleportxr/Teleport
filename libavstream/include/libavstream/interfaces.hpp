// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <memory>

namespace avs
{

class PipelineNode;
class SurfaceBackendInterface;
class GeometryTargetBackendInterface;
class GeometryEncoderBackendInterface;
class GeometryDecoderBackendInterface;
class GeometryRequesterBackendInterface;
class AudioTargetBackendInterface;

/*!
 * General (stream of bytes) I/O node interface.
 *
 * Nodes implementing this interface can act as data sources and/or sinks for the purpose of arbitrary reads and/or writes.
 */
class AVSTREAM_API IOInterface
{
public:
	virtual ~IOInterface() = default;

	/*!
	 * Read bytes.
	 * \param reader PipelineNode which performs the read operation.
	 * \param buffer Pointer to destination buffer.
	 * \param bufferSize Size of destination buffer in bytes.
	 * \param bytesRead Number of bytes actually read should the read operation succeed.
	 * \return
	 *  - Result::OK on success.
	 *  - PipelineNode specific error result on failure.
	 */
	virtual Result read(PipelineNode* reader, void* buffer, size_t& bufferSize, size_t& bytesRead) = 0;

	/*!
	 * Write bytes.
	 * \param writer PipelineNode which performs the write operation.
	 * \param buffer Pointer to soruce buffer.
	 * \param bufferSize Size of source buffer in bytes.
	 * \param bytesWritten Number of bytes actually written should the write operation succeed.
	 * \return
	 *  - Result::OK on success.
	 *  - Result::IO_InvalidArgument if either buffer is nullptr or bufferSize is zero.
	 *  - PipelineNode specific error result.
	 */
	virtual Result write(PipelineNode* writer, const void* buffer, size_t bufferSize, size_t& bytesWritten) = 0;
};

/*!
 * Packet I/O node interface.
 *
 * Nodes implementing this interface can act as data sources and/or sinks for the purpose of packet reads and/or writes.
 * Packet reads (writes) operate on atomic chunks of data - packets cannot be partially read or written.
 */
class AVSTREAM_API PacketInterface
{
public:
	virtual ~PacketInterface() = default;

	// Read packet.
	// \param reader PipelineNode which performs the read operation.
	// \param buffer Pointer to destination buffer (may be nullptr to query for buffer size).
	// \param bufferSize Size of destination buffer in bytes
	// \param streamIndex Stream Index.
	// \return
	//  - Result::OK on success.
	//  - Result::IO_Retry if either buffer is nullptr or bufferSize is too small.
	//                     Correct buffer size is written back to bufferSize and the read should be retried.
	//  - PipelineNode specific error result.
	// 
	virtual Result readPacket(PipelineNode *reader, void *buffer, size_t &bufferSize, int streamIndex) = 0;

	// Write packet.
	// \param writer PipelineNode which performs the write operation.
	// \param buffer Pointer to source buffer.
	// \param bufferSize Size of source buffer in bytes.
	// \param streamIndex Stream Index.
	// \return
	//  - Result::OK on success.
	//  - Result::IO_InvalidArgument if either buffer is nullptr or bufferSize is zero.
	//  - PipelineNode specific error result.
	virtual Result writePacket(PipelineNode *writer, const void *buffer, size_t bufferSize, const int streamIndex) = 0;
};

/*!
 * Surface node interface.
 *
 * Nodes implementing this interface can act as data sources/sinks for the purpose of providing access to a surface.
 */
class AVSTREAM_API SurfaceInterface
{
public:
	virtual ~SurfaceInterface() = default;

	/*! Get surface backend associated with this node. */
	virtual SurfaceBackendInterface* getBackendSurface() const = 0;

	/*! Get surface backend associated with this node. */
	virtual SurfaceBackendInterface* getAlphaBackendSurface() const = 0;
};

/*!
 * Mesh interface.
 *
 * Nodes implementing this interface can act as data sources/sinks for the purpose of providing access to a mesh.
 */
class AVSTREAM_API GeometrySourceInterface
{
public:
	virtual ~GeometrySourceInterface() = default;

	/*! Get  backend associated with this node. */
	virtual GeometryRequesterBackendInterface* getGeometryRequesterBackendInterface() const = 0;
};

/*!
 * Geometry target interface.
 *
 * Nodes implementing this interface can act as data sinks for the purpose of building geometry.
 */
class AVSTREAM_API GeometryTargetInterface
{
public:
	virtual ~GeometryTargetInterface() = default;

	/*! Get surface backend associated with this node. */
	virtual GeometryTargetBackendInterface* getGeometryTargetBackendInterface() const = 0;
};

/*!
 * Audio target interface.
 *
 * Nodes implementing this interface can act as data sinks for the purpose of decoding and playing audio.
 */
class AVSTREAM_API AudioTargetInterface
{
public:
	virtual ~AudioTargetInterface() = default;

	/*! Get audio backend associated with this node. */
	virtual AudioTargetBackendInterface* getAudioTargetBackendInterface() const = 0;
};

} // avs