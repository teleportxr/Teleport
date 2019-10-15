
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

    vec4 lookup = textureLod(cubemapTexture, vSampleVec,0.0);
    vec3 colourSampleVec=vSampleVec;
    for (int i = 0; i < 1; i++)
    {
        float depth = lookup.a;
        float dist_m=max(0.5,20.0*depth);
        vec3 pos_m=dist_m*vDirection;
        pos_m+=vEyeOffset* step(-0.9, -depth);

        // But this does not intersect at depth. We want the vector from the original centre, of

        // original radius to hit point
        float R = dist_m;
        float F = length(vEyeOffset);
        {
            float D = -dot(normalize(vEyeOffset), vDirection);
            float b = F * D;
            float c = F * F - R * R;
            float U = -b + sqrt(b * b - c);
            pos_m += (U - R) * vDirection*step(-F,0.0);

            colourSampleVec  = normalize(vec3(-pos_m.z, pos_m.x, pos_m.y));
            lookup=textureLod(cubemapTexture, colourSampleVec, 0.0);
        }
    }
//finalLookup.rgb+=fract(vEyeOffset);
    gl_FragColor = pow(lookup,vec4(.44,.44,.44,1.0)) + RWCameraPosition[0];
    //8.0*abs(vSampleVec.z)
}
