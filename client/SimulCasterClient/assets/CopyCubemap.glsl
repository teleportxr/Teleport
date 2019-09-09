#version 310 es

uniform ivec2 offset;
layout(rgba8, binding = 0) uniform image2DArray destTex;
uniform samplerExternalOES videoFrameTexture;

layout (local_size_x = 16, local_size_y = 16) in;
void main() {
    ivec3 storePos = ivec3(gl_GlobalInvocationID.xyz);
    imageStore(destTex, storePos, vec4(1.0, 0.0, 0.0, 0.0));
}
