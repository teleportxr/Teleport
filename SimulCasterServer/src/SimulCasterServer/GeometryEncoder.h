#pragma once

#include "libavstream/geometry/mesh_interface.hpp"

namespace SCServer
{
	class GeometryEncoder: public avs::GeometryEncoderBackendInterface
	{
	public:
		int32_t geometryBufferCutoffSize;

		GeometryEncoder();
		~GeometryEncoder() = default;

		// Inherited via GeometryEncoderBackendInterface
		avs::Result encode(uint32_t timestamp, avs::GeometrySourceBackendInterface* target
						   , avs::GeometryRequesterBackendInterface* geometryRequester) override;
		avs::Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) override;
		avs::Result unmapOutputBuffer() override;
	protected:
		std::vector<char> buffer; //Buffer used to encode data before checking it can be sent.
		std::vector<char> queuedBuffer; //Buffer given to the pipeline to be sent to the client.
		template<typename T> size_t put(const T& data)
		{
			size_t pos = buffer.size();
			buffer.resize(buffer.size() + sizeof(T));
			memcpy(buffer.data() + pos, &data, sizeof(T));
			return pos;
		}
		size_t put(const uint8_t* data, size_t count)
		{
			size_t pos = buffer.size();
			buffer.resize(buffer.size() + count);
			memcpy(buffer.data() + pos, data, count);
			return pos;
		}
		template<typename T> void replace(size_t pos, const T& data)
		{
			memcpy(buffer.data() + pos, &data, sizeof(T));
		}
	private:
		void putPayload(avs::GeometryPayloadType t);
		static unsigned char GALU_code[];

		//Following functions push the data from the source onto the buffer, depending on what the requester needs.
		//	src : Source we are taking the data from.
		//	req : Object that defines what needs to transfered across.
		//Returns a code to determine how the encoding went.
		avs::Result encodeMeshes(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeNodes(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeTextures(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeMaterials(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeShadowMaps(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);

		//The actual implementation of encode textures that can be used by encodeMaterials to package textures with it.
		avs::Result encodeTexturesBackend(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs, bool isShadowMap = false);

		//Moves the data from buffer into queuedBuffer; keeping in mind the recommended buffer cutoff size.
		//Data will usually not be queued if it would cause it to exceed the recommended size, but the data may have been queued anyway.
		//This happens when not queueing it would have left queuedBuffer empty.
		//Returns whether the queue attempt did not exceed the recommended buffer size.
		bool attemptQueueData();
	};
}