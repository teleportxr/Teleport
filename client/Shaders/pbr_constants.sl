//  Copyright (c) 2019-2024 Simul Software Ltd. All rights reserved.
#ifndef PBR_CONSTANTS_SL
#define PBR_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(TeleportSceneConstants, 10)
int lightCount;
int reverseDepth;
float drawDistance;
float roughestMip;
SIMUL_CONSTANT_BUFFER_END


PLATFORM_GROUPED_CONSTANT_BUFFER(PbrMaterialConstants, 5, 2)
	vec4 diffuseOutputScalar;
	vec4 normalOutputScalar;
	vec4 combinedOutputScalarRoughMetalOcclusion;
	vec4 emissiveOutputScalar;

	vec2 diffuseTexCoordsScale;
	vec2 normalTexCoordsScale;
	vec2 combinedTexCoordsScale;
	vec2 emissiveTexCoordsScale;

	vec3 u_SpecularColour;
	int diffuseTexCoordIndex;

	int normalTexCoordIndex;
	int combinedTexCoordIndex;
	int emissiveTexCoordIndex;
	int lightmapTexCoordIndex;

PLATFORM_GROUPED_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(PerNodeConstants, 11)
uniform mat4 model;
vec4 lightmapScaleOffset;
float rezzing;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(BoneMatrices, 12)
	mat4 boneMatrices[64];
SIMUL_CONSTANT_BUFFER_END

struct PbrLight
{
	mat4 lightSpaceTransform;
	vec4 colour;
	vec3 position;
	float power;
	vec3 direction;
	float is_point;
	float is_spot;
	float radius;
	uint uid32;		// lowest 32 bits of the uid.
	float pad1;
};						

#endif
