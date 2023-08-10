//  Copyright (c) 2019 Simul Software Ltd. All rights reserved.
#ifndef PBR_CONSTANTS_SL
#define PBR_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(PbrConstants,13)
	int lightCount;
	int reverseDepth;
	float drawDistance;
	float roughestMip;

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

SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(PerNodeConstants, 11)
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
