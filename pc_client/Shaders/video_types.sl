//  Copyright (c) 2020 Simul Software Ltd. All rights reserved.
#ifndef VIDEO_TYPES_SL
#define VIDEO_TYPES_SL

// Placeholders for lights
struct DirectionalLight
{
	vec3 direction;
	float pad;
	vec4 color;
	mat4 shadowViewMatrix;
	mat4 shadowProjectionMatrix;
};

struct PointLight
{
	vec3 position;
	float range;
	vec3 attenuation;
	float pad;
	vec4 color;
	mat4 shadowViewMatrix;
	mat4 shadowProjectionMatrix;
};

struct SpotLight
{
	vec3 position;
	float  range;
	vec3 direction;
	float cone;
	vec3 attenuation;
	float pad;
	vec4 color;
	mat4 shadowViewMatrix;
	mat4 shadowProjectionMatrix;
};

struct LightTag
{
	mat4 worldToShadowMatrix;
	vec2 shadowTexCoordOffset;
	vec2 shadowTexCoordScale;
	vec3 position;
	int	pospad;
	vec3 direction;
	int dirpad;
};

struct VideoTagData2D
{
	vec3 cameraPosition;
	float pad;
	vec4 cameraRotation;
	// Some light information 
	LightTag lightTags[4];
};

struct VideoTagDataCube
{
	vec3 cameraPosition;
	float pad;
	vec4 cameraRotation;
	// Some light information 
	LightTag lightTags[4];
};

#endif
