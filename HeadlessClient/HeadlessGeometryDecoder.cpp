#include "HeadlessGeometryDecoder.h"
#include "HeadlessGeometryCacheBackend.h"
#include "TeleportCore/Logging.h"
#include <libavstream/common.hpp>
#include <libavstream/node.h>

// The node, skeleton and pointer wire formats mirror clientrender::GeometryDecoder's
// decodeNode()/decodeSkeleton()/decodeMeshPointer(). Keep them in step if the protocol changes.

HeadlessGeometryDecoder::HeadlessGeometryDecoder(HeadlessGeometryCacheBackend *c)
	: cache(c)
{
}

avs::Result HeadlessGeometryDecoder::decode(avs::uid							 server_uid,
											const void						*buffer,
											size_t							 bufferSizeInBytes,
											avs::GeometryPayloadType		 type,
											avs::GeometryTargetBackendInterface *target,
											avs::uid						 uid)
{
	if (!buffer || !bufferSizeInBytes)
		return avs::Result::Failed;

	Reader r;
	r.data = (const uint8_t *)buffer;
	r.size = bufferSizeInBytes;

	// Acknowledge first, and unconditionally. RemoveNodes carries no uid of its own, and for
	// everything else the server's obligation ends when the payload reaches us - whether or not
	// we go on to parse the body.
	if (cache && type != avs::GeometryPayloadType::RemoveNodes)
		cache->ReceivedResource(uid);

	switch (type)
	{
	case avs::GeometryPayloadType::Node:
		return DecodeNode(r, server_uid, uid, target);
	case avs::GeometryPayloadType::RemoveNodes:
		return DecodeRemoveNodes(r, server_uid, target);
	case avs::GeometryPayloadType::Skeleton:
		return DecodeSkeleton(r, server_uid, uid, target);
	case avs::GeometryPayloadType::MeshPointer:
	case avs::GeometryPayloadType::TexturePointer:
	case avs::GeometryPayloadType::MaterialPointer:
	case avs::GeometryPayloadType::AnimationPointer:
		return DecodePointer(r, uid, type);
	default:
		// Mesh, Material, MaterialInstance, Texture, Animation, FontAtlas, TextCanvas: recorded
		// as received above, body intentionally not parsed. Decoding these would mean draco, ktx
		// and image codecs, which is what we are avoiding.
		if (cache)
			cache->CountUnparsedPayload(type);
		return avs::Result::OK;
	}
}

avs::Result HeadlessGeometryDecoder::DecodeNode(Reader &r, avs::uid server_uid, avs::uid uid, avs::GeometryTargetBackendInterface *target)
{
	avs::Node node;

	if (!r.ReadString(node.name))
	{
		TELEPORT_WARN("Geometry: node {} has a malformed name", uid);
		return avs::Result::Failed;
	}
	node.localTransform	  = r.Get<avs::Transform>();
	node.stationary		  = r.Get<uint8_t>() != 0;
	node.holder_client_id = r.Get<uint64_t>();
	node.priority		  = r.Get<int32_t>();
	node.parentID		  = r.Get<uint64_t>();

	const size_t numComponents = (size_t)r.Get<uint8_t>();
	if (!r.ok)
	{
		TELEPORT_WARN("Geometry: node {} truncated before components", uid);
		return avs::Result::Failed;
	}

	for (size_t i = 0; i < numComponents; i++)
	{
		node.data_type = static_cast<avs::NodeDataType>(r.Get<uint8_t>());
		if (!r.ok)
			break;

		switch (node.data_type)
		{
		case avs::NodeDataType::Mesh:
			node.data_uid	= r.Get<uint64_t>();
			node.skeletonID = r.Get<uint64_t>();
			if (!r.ReadList<uint16_t, int16_t>(node.joint_indices))
				break;
			if (!r.ReadList<uint16_t, avs::uid>(node.animations))
				break;
			if (!r.ReadList<uint16_t, avs::uid>(node.materials))
				break;
			node.renderState.lightmapScaleOffset  = r.Get<vec4>();
			node.renderState.globalIlluminationUid = r.Get<uint64_t>();
			break;
		case avs::NodeDataType::Light:
			node.lightColour	= r.Get<vec4>();
			node.lightRadius	= r.Get<float>();
			node.lightRange		= r.Get<float>();
			node.lightDirection = r.Get<vec3>();
			node.lightType		= r.Get<uint8_t>();
			break;
		case avs::NodeDataType::Link:
			if (!r.ReadString(node.url))
				break;
			if (!r.ReadString(node.query_url))
				break;
			break;
		case avs::NodeDataType::TextCanvas:
			node.data_uid = r.Get<uint64_t>();
			break;
		default:
			// An unrecognised component type has an unknown length, so we cannot safely keep
			// reading this payload. Everything parsed so far is still valid.
			TELEPORT_WARN("Geometry: node {} component {} has unhandled type {}; stopping parse", uid, i, (int)node.data_type);
			r.ok = false;
			break;
		}
		if (!r.ok)
			break;
	}

	if (!r.ok)
	{
		TELEPORT_WARN("Geometry: node {} \"{}\" was truncated or malformed", uid, node.name);
		return avs::Result::Failed;
	}

	if (target)
		target->CreateNode(server_uid, uid, node);
	return avs::Result::OK;
}

avs::Result HeadlessGeometryDecoder::DecodeRemoveNodes(Reader &r, avs::uid server_uid, avs::GeometryTargetBackendInterface *target)
{
	const uint16_t num = r.Get<uint16_t>();
	if (!r.ok || !r.Remaining(num * sizeof(avs::uid)))
	{
		TELEPORT_WARN("Geometry: RemoveNodes claims {} uids but the payload is too short", num);
		return avs::Result::Failed;
	}
	for (uint16_t i = 0; i < num; i++)
	{
		const avs::uid u = r.Get<uint64_t>();
		if (target)
			target->DeleteNode(server_uid, u);
	}
	return avs::Result::OK;
}

avs::Result HeadlessGeometryDecoder::DecodeSkeleton(Reader &r, avs::uid server_uid, avs::uid uid, avs::GeometryTargetBackendInterface *target)
{
	avs::Skeleton skeleton;
	if (!r.ReadString(skeleton.name))
	{
		TELEPORT_WARN("Geometry: skeleton {} has a malformed name", uid);
		return avs::Result::Failed;
	}
	const size_t numBones = (size_t)r.Get<uint64_t>();
	if (!r.ok || !r.Remaining(numBones * sizeof(avs::uid)))
	{
		TELEPORT_WARN("Geometry: skeleton {} claims {} bones but the payload is too short", uid, numBones);
		return avs::Result::Failed;
	}
	skeleton.boneIDs.resize(numBones);
	for (size_t i = 0; i < numBones; i++)
	{
		skeleton.boneIDs[i] = r.Get<uint64_t>();
	}
	if (target)
		target->CreateSkeleton(server_uid, uid, skeleton);
	return avs::Result::OK;
}

avs::Result HeadlessGeometryDecoder::DecodePointer(Reader &r, avs::uid uid, avs::GeometryPayloadType type)
{
	std::string url;
	if (!r.ReadString(url))
	{
		TELEPORT_WARN("Geometry: {} {} has a malformed URL", avs::stringOf(type), uid);
		return avs::Result::Failed;
	}
	if (cache)
		cache->TrackPointer(uid, type, url);
	// Mesh and animation pointers carry the axes standard the asset is authored in, appended
	// after the url. It is optional, so its absence is normal rather than an error, and means
	// "the same standard as the server's scene".
	uint8_t axesStandard = 0;
	if (type == avs::GeometryPayloadType::MeshPointer || type == avs::GeometryPayloadType::AnimationPointer)
	{
		if (r.Remaining(1))
			axesStandard = r.Get<uint8_t>();
	}
	// No fetch: the uid was acknowledged in decode(), which is all the server is waiting for.
	TELEPORT_LOG("Geometry: {} {} -> {} (axes={}, recorded, not downloaded)",
				 avs::stringOf(type), uid, url, axesStandard ? std::to_string(axesStandard) : std::string("server's own"));
	return avs::Result::OK;
}
