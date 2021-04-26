//  Copyright (c) 2016 Simul Software Ltd. All rights reserved.
#ifndef CUBEMAP_CONSTANTS_SL
#define CUBEMAP_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(CubemapConstants, 4)
uniform mat4 serverProj;
uniform vec3 offsetFromVideo;
uniform int _pad1;
uniform vec3 cameraPosition;
uniform int _pad2;
uniform vec4 cameraRotation;
uniform vec4 depthOffsetScale;
uniform int2 sourceOffset;
uniform int targetSize;

SIMUL_CONSTANT_BUFFER_END

#ifndef __cplusplus
struct posTexVertexOutputDebug
{
	vec4 hPosition		: SV_POSITION;
	vec2 texCoords		: TEXCOORD0;	
	vec3 color          : TEXCOORD1;
};
#endif

#endif
