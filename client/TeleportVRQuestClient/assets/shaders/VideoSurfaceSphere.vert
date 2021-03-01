precision highp float;

layout(location = 0) in vec4 position;

layout(location = 0) out vec3 vSampleVec;
layout(location = 5) out vec3 vEyeOffset;
layout(location = 6) out vec3 vDirection;
layout(location = 7) out vec2 vTexCoords;

layout(std140, binding = 1) uniform videoUB
{
    vec4 eyeOffsets[2];
    mat4 invViewProj[2];
    mat4 viewProj;
    vec3 cameraPosition;
    int _pad2;
} vid;

void main()
{
    vDirection      =normalize(position.xyz);
    // Equirect map sampling vector is rotated -90deg on Y axis to match UE4 yaw.
    vSampleVec      =normalize(vec3(-position.z,position.x,position.y));
    vec4 eye_pos    =vec4((sm.ViewMatrix[VIEW_ID] * ( vec4(position.xyz,0.0) )).xyz,1.0);
    vec4 out_pos    =sm.ProjectionMatrix[VIEW_ID] * eye_pos;
    //vec4 out_pos    =vid.viewProj*position;
    gl_Position     =out_pos;//vec4(out_pos.xy/out_pos.z,0.0,1.0);
    vEyeOffset      =vid.eyeOffsets[VIEW_ID].xyz;
}
