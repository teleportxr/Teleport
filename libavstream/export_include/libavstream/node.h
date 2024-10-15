#pragma once

#include <libavstream/common_networking.h>
#include <libavstream/common_packing.h>
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
namespace avs
{
	extern void AVSTREAM_API ConvertTransform(AxesStandard fromStandard, AxesStandard toStandard, Transform &transform);
	extern void AVSTREAM_API ConvertRotation(AxesStandard fromStandard, AxesStandard toStandard, vec4 &rotation);
	extern void AVSTREAM_API ConvertPosition(AxesStandard fromStandard, AxesStandard toStandard, vec3 &position);
	extern void AVSTREAM_API ConvertScale(AxesStandard fromStandard, AxesStandard toStandard, vec3 &scale);
	extern int8_t AVSTREAM_API ConvertAxis(AxesStandard fromStandard, AxesStandard toStandard, int8_t axis);
	
	struct Node
	{
		std::string name;

		Transform localTransform;

		bool stationary=false;

		uid holder_client_id=0;

		int32_t priority=0;

		uid parentID=0;

		// The following should be separated out into node components:
		NodeDataType data_type=NodeDataType::None;
		uid data_uid=0;

		// Mesh: materials for the submeshes.
		std::vector<uid> materials;

		//SKINNED MESH
		uid skeletonNodeID=0;
		std::vector<int16_t> joint_indices;
		std::vector<uid> animations;

		// e.g. lightmap offset and multiplier.
		NodeRenderState renderState;

		//LIGHT
		vec4 lightColour	={0,0,0,0};
		float lightRadius	=0.f;
		vec3 lightDirection	={0,0,1.0f};	// Unchanging rotation that orients the light's shadowspace so that it shines on the Z axis with X and Y for shadowmap.
		uint8_t lightType	=0;
		float lightRange	=0.f;			//! Maximum distance the light is effective at, in metres.

		std::string url;		// if node is a link/portal
		std::string query_url; // if node is a link/portal
	} AVS_PACKED;
	static_assert (sizeof(Node) == 308, "avs::Node size is not correct");
}
#ifdef _MSC_VER
#pragma pack(pop)
#endif