//  Copyright (c) 2019 Simul Software Ltd. All rights reserved.
#ifndef PBR_CONSTANTS_SL
#define PBR_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(PbrConstants,13)
	int lightCount;
	int reverseDepth;
	float drawDistance;
	float roughestMip;

	vec4 depthToLinFadeDistParams;

	vec4 diffuseOutputScalar;
	vec2 diffuseTexCoordsScalar_R;
	vec2 diffuseTexCoordsScalar_G;
	vec2 diffuseTexCoordsScalar_B;
	vec2 diffuseTexCoordsScalar_A;

	vec4 normalOutputScalar;
	vec2 normalTexCoordsScalar_R;
	vec2 normalTexCoordsScalar_G;
	vec2 normalTexCoordsScalar_B;
	vec2 normalTexCoordsScalar_A;

	vec4 combinedOutputScalarRoughMetalOcclusion;
	vec2 combinedTexCoordsScalar_R;
	vec2 combinedTexCoordsScalar_G;
	vec2 combinedTexCoordsScalar_B;
	vec2 combinedTexCoordsScalar_A;

	vec4 emissiveOutputScalar;
	vec2 emissiveTexCoordsScalar_R;
	vec2 emissiveTexCoordsScalar_G;
	vec2 emissiveTexCoordsScalar_B;
	vec2 emissiveTexCoordsScalar_A;

	vec3 u_SpecularColour;
	float pad135;

	float u_DiffuseTexCoordIndex;
	float u_NormalTexCoordIndex;
	float u_CombinedTexCoordIndex;
	float u_EmissiveTexCoordIndex;
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
