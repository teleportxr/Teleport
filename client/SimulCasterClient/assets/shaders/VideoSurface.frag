
precision highp float;

const float PI    = 3.1415926536;
const float TwoPI = 2.0 * PI;

uniform samplerCube cubemapTexture;
uniform samplerExternalOES videoFrameTexture;

layout(location = 0) in vec3 vSampleVec;
layout(location = 1) in mat4 vInvViewProj;
layout(location = 5) in vec3 vEyeOffset;
layout(location = 6) in vec3 vDirection;
layout(location = 7) in vec2 vTexCoords;

layout(std140, binding = 2) uniform videoUB
{
    vec4 eyeOffsets[2];
    mat4 invViewProj[2];
    vec3 cameraPosition;
    int _pad2;
} vid;


layout(std430,binding=3) buffer RWCameraPosition_ssbo
{
    vec4 RWCameraPosition[8];
};


vec2 WorldspaceDirToUV(vec3 wsDir)
{
    float phi   = atan(wsDir.z, wsDir.x) / TwoPI;
    float theta = acos(wsDir.y) / PI;
    vec2  uv    = fract(vec2(phi, theta));
    return uv;
}
vec2 OffsetScale(vec2 uv, vec4 offsc)
{
    return uv * offsc.zw + offsc.xy;
}
vec4 mul(mat4 a,vec4 b)
{
    return a*b;
}
void main()
{
    vec4 clip_pos=vec4(-1.0,-1.0,1.0,1.0);
    clip_pos.x+=2.0*vTexCoords.x;
    clip_pos.y+=2.0*vTexCoords.y;
   //vec3 view=normalize((clip_pos*vInvViewProj).xyz);

    vec3 offsetFromVideo2=vid.cameraPosition+vEyeOffset-RWCameraPosition[0].xyz;
    vec4 lookup = textureLod(cubemapTexture, vSampleVec,0.0);
    vec3 view = vSampleVec;
    vec3 colourSampleVec=vSampleVec;
    for (int i = 0; i < 5; i++)
    {
        float depth = lookup.a;
        float dist_m=max(0.2,20.0*depth);
        vec3 pos_m=dist_m*vDirection;
        pos_m+=offsetFromVideo2* step(-0.8, -depth);

        // But this does not intersect at depth. We want the vector from the original centre, of

        // original radius to hit point
        float R = dist_m;
        float F = length(offsetFromVideo2);
        {
            float D = -dot(normalize(offsetFromVideo2), vDirection);
            float b = F * D;
            float c = F * F - R * R;
            float U = -b + sqrt(b * b - c);
            pos_m += (U - R) * vDirection*step(-F,0.0);

            colourSampleVec  = normalize(vec3(-pos_m.z, pos_m.x, pos_m.y));
            lookup=textureLod(cubemapTexture, colourSampleVec, 0.0);
        }
    }
//finalLookup.rgb+=fract(vEyeOffset);
    gl_FragColor = pow(lookup,vec4(.44,.44,.44,1.0));//+ max(vec4(0,0,0,0),vec4(normalize(offsetFromVideo2.xyz),0));
    //8.0*abs(vSampleVec.z)
}
