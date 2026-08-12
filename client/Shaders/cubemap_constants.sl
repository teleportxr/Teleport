//  Copyright (c) 2016 Simul Software Ltd. All rights reserved.
#ifndef CUBEMAP_CONSTANTS_SL
#define CUBEMAP_CONSTANTS_SL

PLATFORM_NAMED_CONSTANT_BUFFER(CubemapConstants, cubemapConstants,4,3)
uniform mat4 serverProj;
uniform vec3 offsetFromVideo;
uniform float time_seconds;
uniform vec3 cameraPosition;
uniform int _pad2;
uniform vec4 cameraRotation;
uniform vec4 depthOffsetScale;
uniform int2 sourceOffset;
uniform int2 targetSize;
// Takes a sample direction from the client's own frame into the frame the background
// cubemap's faces are laid out in. Identity unless the two disagree; see the TexturePointer
// axes-standard byte in docs/protocol/geometry_payload.rst. The 16 bytes above keep this on
// the 16-byte boundary a mat4 needs.
uniform mat4 cubemapAxesTransform;

PLATFORM_NAMED_CONSTANT_BUFFER_END

#endif
