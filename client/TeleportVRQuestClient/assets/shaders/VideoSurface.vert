precision highp float;

layout(location = 0) in vec4 position;

layout(location = 0) out vec3 vSampleVec;
layout(location = 1) out mat4 vInvViewProj;
layout(location = 5) out vec3 vEyeOffset;
//layout(location = 6) out vec3 vDirection;
layout(location = 7) out vec2 vTexCoords;

layout(std140, binding = 1) uniform videoUB
{
    vec4 eyeOffsets[2];
    mat4 invViewProj[2];
    mat4 viewProj;
    mat4 serverProj;
    vec3 cameraPosition;
    int _pad2;
} vid;

void main() {

   vInvViewProj=vid.invViewProj[VIEW_ID];

    vec2 poss[4];
    poss[0]         =vec2(1.0,0.0);
    poss[1]         =vec2(1.0,1.0);
    poss[2]         =vec2(0.0,0.0);
    poss[3]         =vec2(0.0,1.0);
    vec2 pos        =poss[gl_VertexID];
    vTexCoords=pos;
    vec4 rect       =vec4(-1.0,-1.0,2.0,2.0);
    gl_Position   =vec4(rect.xy+rect.zw*pos,0.0,1.0);
    //vDirection      =normalize(position.xyz);
    // Equirect map sampling vector is rotated -90deg on Y axis to match UE4 yaw.
    vSampleVec      =normalize(vec3(-position.z,position.x,position.y));
    //vec4 eye_pos    =(sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * position ));
    //gl_Position     =(sm.ProjectionMatrix[VIEW_ID] * eye_pos);
  //  vEyespacePos    =eye_pos.xyz;

    //
    //vec3 x_vector   =sm.ViewMatrix[VIEW_ID] * vec4( 1.0,0,0,0 );
    // The view offset in worldspace from the video centre:
    vEyeOffset      =vid.eyeOffsets[VIEW_ID].xyz;
    //vEyeOffset		+=x_vector*0.08*(float(VIEW_ID)-0.5);


/*
    vec2 poss[4];
    poss[0]=vec2(1.0,-1.0);
    poss[1]=vec2(1.0,1.0);
    poss[2]=vec2(-1.0,-1.0);
    poss[3]=vec2(-1.0,1.0);
    vec2 pos=poss[gl_VertexID];
    gl_Position=vec4(pos,0.0,1.0);
    vTexCoords=0.5*(vec2(1.0,1.0)+vec2(pos.x,pos.y));*/
}
