precision lowp float;

layout(location = 0) in vec4 position;

layout(location = 0) out vec3 vSampleVec;
layout(location = 7) out vec3 vOffsetFromVideo;

layout(std140, binding = 1) uniform videoUB
{
    vec4 eyeOffsets[2];
    mat4 invViewProj[2];
    mat4 viewProj;
    vec3 cameraPosition;
    int _pad2;
} vid;

// ALL light data is passed in as tags.
struct LightTag
{
    mat4 worldToShadowMatrix;
    vec2 shadowTexCoordOffset;
    vec2 shadowTexCoordScale;
    vec4 colour;
    vec3 position;
    int	pospad;
    vec3 direction;
    uint uid32;
    float is_spot;
    float is_point;
    float shadow_strength;
    float radius;
};
struct VideoTagDataCube
{
    vec3 cameraPosition;
    int lightCount;
    vec4 cameraRotation;
    LightTag lightTags[4];
};

layout(std430, binding = 0) buffer TagDataCube_ssbo
{
    VideoTagDataCube tagDataCube;
};
void main()
{
    // Equirect map sampling vector is rotated -90deg on Y axis to match UE4 yaw.
    vSampleVec      =normalize(vec3(-position.z,position.x,position.y));
    vec4 eye_pos    =vec4((sm.ViewMatrix[VIEW_ID] * ( vec4(position.xyz,0.0) )).xyz,1.0);
    vec4 out_pos    =sm.ProjectionMatrix[VIEW_ID] * eye_pos;
    //vec4 out_pos    =vid.viewProj*position;
    gl_Position     =out_pos;//vec4(out_pos.xy/out_pos.z,0.0,1.0);
    vec3 eyeOffset  =vid.eyeOffsets[VIEW_ID].xyz;
    vec3 v_offs     =vid.cameraPosition-tagDataCube.cameraPosition+eyeOffset;
    vOffsetFromVideo=vec3(-v_offs.z,v_offs.x,v_offs.y)*5.0;
}
