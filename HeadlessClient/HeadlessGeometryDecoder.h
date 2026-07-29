#pragma once

#include <libavstream/geometry/mesh_interface.hpp>
#include <cstdint>
#include <string>
#include <vector>

class HeadlessGeometryCacheBackend;

//! Parses geometry payloads without decompressing or downloading any asset data.
//!
//! ClientRender uses clientrender::GeometryDecoder for this, but that class lives in a library
//! which links a graphics API and is welded to clientrender::ResourceCreator, so it cannot be
//! reused here. The framing is already handled for us: avs::GeometryDecoder classifies the
//! payload, extracts the uid and validates sizes before calling decode() below.
//!
//! Payloads are handled at three levels of detail:
//!   - Node / RemoveNodes / Skeleton are parsed in full, so the node graph is tracked.
//!   - MeshPointer / TexturePointer / MaterialPointer have their URL read and recorded; the
//!     asset is deliberately never fetched.
//!   - Everything else is acknowledged by uid and its body skipped.
class HeadlessGeometryDecoder final : public avs::GeometryDecoderBackendInterface
{
public:
	explicit HeadlessGeometryDecoder(HeadlessGeometryCacheBackend *cache);
	virtual ~HeadlessGeometryDecoder() = default;

	avs::Result decode(avs::uid						  server_uid,
					   const void					 *buffer,
					   size_t						  bufferSizeInBytes,
					   avs::GeometryPayloadType		  type,
					   avs::GeometryTargetBackendInterface *target,
					   avs::uid						  uid) override;

private:
	//! Cursor over one payload body. Every read is bounds-checked; a short payload fails the
	//! decode rather than reading past the buffer.
	struct Reader
	{
		const uint8_t *data = nullptr;
		size_t		   size = 0;
		size_t		   offset = 0;
		bool		   ok = true;

		bool Remaining(size_t bytes) const { return ok && offset + bytes <= size; }

		template <typename T> T Get()
		{
			if (!Remaining(sizeof(T)))
			{
				ok = false;
				return T();
			}
			T t;
			memcpy(&t, data + offset, sizeof(T));
			offset += sizeof(T);
			return t;
		}

		bool ReadString(std::string &str)
		{
			uint16_t length = Get<uint16_t>();
			if (!ok || !Remaining(length))
			{
				ok = false;
				return false;
			}
			str.assign((const char *)(data + offset), length);
			offset += length;
			return true;
		}

		template <typename CountType, typename ElementType> bool ReadList(std::vector<ElementType> &list)
		{
			CountType count = Get<CountType>();
			if (!ok)
				return false;
			const size_t bytes = (size_t)count * sizeof(ElementType);
			if (!Remaining(bytes))
			{
				ok = false;
				return false;
			}
			list.resize((size_t)count);
			if (count)
				memcpy(list.data(), data + offset, bytes);
			offset += bytes;
			return true;
		}
	};

	avs::Result DecodeNode(Reader &r, avs::uid server_uid, avs::uid uid, avs::GeometryTargetBackendInterface *target);
	avs::Result DecodeRemoveNodes(Reader &r, avs::uid server_uid, avs::GeometryTargetBackendInterface *target);
	avs::Result DecodeSkeleton(Reader &r, avs::uid server_uid, avs::uid uid, avs::GeometryTargetBackendInterface *target);
	avs::Result DecodePointer(Reader &r, avs::uid uid, avs::GeometryPayloadType type);

	HeadlessGeometryCacheBackend *cache = nullptr;
};
