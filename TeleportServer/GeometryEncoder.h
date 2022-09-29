#pragma once

#include "libavstream/geometry/mesh_interface.hpp"

namespace avs
{
struct FloatKeyframe;
struct Vector3Keyframe;
struct Vector4Keyframe;
}

namespace teleport
{
	class GeometryEncoder: public avs::GeometryEncoderBackendInterface
	{
	public:
		GeometryEncoder(const struct ServerSettings* settings);
		~GeometryEncoder() = default;

		// Inherited via GeometryEncoderBackendInterface
		avs::Result encode(uint64_t timestamp, avs::GeometrySourceBackendInterface* target
						   , avs::GeometryRequesterBackendInterface* geometryRequester) override;
		avs::Result mapOutputBuffer(void*& bufferPtr, size_t& bufferSizeInBytes) override;
		avs::Result unmapOutputBuffer() override;
		void setMinimumPriority(int32_t) override;
	protected:
		std::vector<char> buffer;			//Buffer used to encode data before checking it can be sent.
		std::vector<char> queuedBuffer;		//Buffer given to the pipeline to be sent to the client.
		template<typename T> size_t put(const T& data)
		{
			size_t pos = buffer.size();
			buffer.resize(buffer.size() + sizeof(T));
			memcpy(buffer.data() + pos, &data, sizeof(T));
			return pos;
		}
		size_t put(const uint8_t* data, size_t count);
		template<typename T> void replace(size_t pos, const T& data)
		{
			memcpy(buffer.data() + pos, &data, sizeof(T));
		}
	private:
		const struct ServerSettings* settings;
		size_t prevBufferSize;
		int32_t minimumPriority=0;
		void putPayload(avs::GeometryPayloadType t);
		void putPayloadSize();

		//Following functions push the data from the source onto the buffer, depending on what the requester needs.
		//	src : Source we are taking the data from.
		//	req : Object that defines what needs to transfered across.
		//Returns a code to determine how the encoding went.
		avs::Result encodeAnimation(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, avs::uid animationID);
		avs::Result encodeMaterials(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeMeshes(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeNodes(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeShadowMaps(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		avs::Result encodeSkin(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, avs::uid skinID);
		avs::Result encodeTextures(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs);
		
		//The actual implementation of encode textures that can be used by encodeMaterials to package textures with it.
		avs::Result encodeTexturesBackend(avs::GeometrySourceBackendInterface* src, avs::GeometryRequesterBackendInterface* req, std::vector<avs::uid> missingUIDs, bool isShadowMap = false);

		avs::Result encodeFloatKeyframes(const std::vector<avs::FloatKeyframe>& keyframes);
		avs::Result encodeVector3Keyframes(const std::vector<avs::Vector3Keyframe>& keyframes);
		avs::Result encodeVector4Keyframes(const std::vector<avs::Vector4Keyframe>& keyframes);

		//Moves the data from buffer into queuedBuffer; keeping in mind the recommended buffer cutoff size.
		//Data will usually not be queued if it would cause it to exceed the recommended size, but the data may have been queued anyway.
		//This happens when not queueing it would have left queuedBuffer empty.
		//Returns whether the queue attempt did not exceed the recommended buffer size.
		bool attemptQueueData();
	};
}