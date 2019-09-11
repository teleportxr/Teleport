precision highp float;

layout(location = 0) in vec4 position;

layout(location = 0) out vec3 vSampleVec;
layout(location = 1) out vec3 vEyespacePos;
layout(location = 2) out mat3 vModelViewOrientationMatrixT;
layout(location = 5) out float vEyeOffset;

void main() {

    mat3 ViewOrientationMatrix=mat3(sm.ViewMatrix[VIEW_ID]);
    mat3 ModelOrientationMatrix=mat3(ModelMatrix);
    vModelViewOrientationMatrixT=transpose(ViewOrientationMatrix * ModelOrientationMatrix);

    // Equirect map sampling vector is rotated -90deg on Y axis to match UE4 yaw.
    vSampleVec  = normalize(vec3(-position.z, position.y, position.x));
    vec4 eye_pos= ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * position ));
    gl_Position     = (sm.ProjectionMatrix[VIEW_ID] * eye_pos);
    vEyespacePos    =eye_pos.xyz;

    vEyeOffset		=0.08*(float(VIEW_ID)-0.5);
}