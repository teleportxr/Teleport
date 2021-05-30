
precision lowp float;

//layout(location = 0) in vec3 vSampleVec;
//layout(location = 1) in mat4 vInvViewProj;
//layout(location = 5) in vec3 vEyeOffset;
//layout(location = 6) in vec3 vDirection;
layout(location = 7) in vec2 vTexCoords;

layout(binding = 0) uniform sampler2D videoTexture;
layout(std140, binding = 1) uniform webcamUB
{
    uvec2 sourceTexSize;
    // Offset of webcam texture in video texture
    ivec2 sourceOffset;
    uvec2 camTexSize;
} webcam;

void main()
{
    vec4 lookup = texture(videoTexture, vTexCoords);
    gl_FragColor = pow(lookup, vec4(.44, .44, .44, 1.0));
}
