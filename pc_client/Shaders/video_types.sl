//  Copyright (c) 2020 Simul Software Ltd. All rights reserved.
#ifndef VIDEO_TYPES_SL
#define VIDEO_TYPES_SL

// Placeholders for lights
struct DirectionalLight
{
	vec3 direction;
	float pad;
	vec4 color;
	mat4d shadowViewMatrix;
	mat4d shadowProjectionMatrix;
};

struct PointLight
{
	vec3 position;
	float range;
	vec3 attenuation;
	float pad;
	vec4 color;
	mat4d shadowViewMatrix;
	mat4d shadowProjectionMatrix;
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
	mat4d shadowViewMatrix;
	mat4d shadowProjectionMatrix;
};

struct VideoTagData2D
{
	vec3 cameraPosition;
	float pad;
	vec4 cameraRotation;
};

struct VideoTagDataCube
{
	vec3 cameraPosition;
	float pad;
	vec4 cameraRotation;
	// Some light information 
};

#endif
