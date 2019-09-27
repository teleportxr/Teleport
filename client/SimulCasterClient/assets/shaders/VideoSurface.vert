precision highp float;

layout(location = 0) in vec4 position;

layout(location = 0) out vec3 vSampleVec;
layout(location = 1) out vec3 vEyespacePos;
layout(location = 2) out mat3 vModelViewOrientationMatrixT;
layout(location = 5) out vec3 vEyeOffset;
layout(location = 6) out vec3 vDirection;

layout(std140, binding = 2) uniform videoUB
{
    vec4 eyeOffsets[2];
} vid;

void main() {

    mat3 ViewOrientationMatrix=mat3(sm.ViewMatrix[VIEW_ID]);
    mat3 ModelOrientationMatrix=mat3(ModelMatrix);
    vModelViewOrientationMatrixT=transpose(ViewOrientationMatrix * ModelOrientationMatrix);
    vDirection      =normalize(position.xyz);
    // Equirect map sampling vector is rotated -90deg on Y axis to match UE4 yaw.
    vSampleVec      =normalize(vec3(-position.z,position.x,position.y));
    vec4 eye_pos    =(sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * position ));
    gl_Position     =(sm.ProjectionMatrix[VIEW_ID] * eye_pos);
    vEyespacePos    =eye_pos.xyz;

    //
    //vec3 x_vector   =sm.ViewMatrix[VIEW_ID] * vec4( 1.0,0,0,0 );
    // The view offset in worldspace from the video centre:
    vEyeOffset      =vid.eyeOffsets[VIEW_ID].xyz;
    //vEyeOffset		+=x_vector*0.08*(float(VIEW_ID)-0.5);
}
