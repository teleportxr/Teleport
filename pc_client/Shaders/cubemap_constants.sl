//  Copyright (c) 2016 Simul Software Ltd. All rights reserved.
#ifndef CUBEMAP_CONSTANTS_SL
#define CUBEMAP_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(CubemapConstants, 4)
uniform vec3 offsetFromVideo;
uniform int _pad;
uniform vec4 depthOffsetScale;
uniform int2 sourceOffset;
uniform int targetSize;
SIMUL_CONSTANT_BUFFER_END


#endif
