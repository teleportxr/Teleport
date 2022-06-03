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
uniform int2 targetSize;

SIMUL_CONSTANT_BUFFER_END

#endif
