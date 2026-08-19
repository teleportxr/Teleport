//  Copyright (c) 2019-2024 Simul Software Ltd. All rights reserved.
#ifndef PBR_CONSTANTS_SL
#define PBR_CONSTANTS_SL

PLATFORM_GROUPED_CONSTANT_BUFFER(TeleportSceneConstants, 10, 0)
int lightCount;
int reverseDepth;
float drawDistance;
float roughestMip;
int debugHighlightBone;
// Pad out to a 16-byte boundary: a mat4 must start on one.
int _pad0;
int _pad1;
int _pad2;
// Take an environment sample direction from the client's own frame into the frame each
// cubemap's faces are laid out in. The server declares that frame in the TexturePointer's
// axes-standard byte and never reprojects the file, so the lookup is converted instead.
// Identity when the frames agree, which is the usual case. The two are separate because
// diffuse and specular are separate resources and may be declared differently.
mat4 diffuseCubemapAxesTransform;
mat4 specularCubemapAxesTransform;
PLATFORM_GROUPED_CONSTANT_BUFFER_END


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

PLATFORM_NAMED_CONSTANT_BUFFER(PerNodeConstants, perNodeConstants, 11, 3)
uniform mat4 model;
vec4 lightmapScaleOffset;
float rezzing;
PLATFORM_NAMED_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(BoneMatrices, 12)
	mat4 boneMatrices[128];
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(LinkConstants, 13)
	vec4 linkColour;
	float radius;
	float time;
	float distanceToDepthParam;
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

struct SurfaceState
{
	vec3 F;
	vec3 kS;
	vec3 kD;
	vec3 refl;
	float n_v;
};
#endif
