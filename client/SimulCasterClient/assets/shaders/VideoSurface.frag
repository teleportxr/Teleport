
precision highp float;

const float PI    = 3.1415926536;
const float TwoPI = 2.0 * PI;

uniform vec4 colourOffsetScale;
uniform vec4 depthOffsetScale;
uniform samplerCube cubemapTexture;
uniform samplerExternalOES videoFrameTexture;

layout(location = 0) in vec3 vSampleVec;
layout(location = 1) in vec3 vEyespacePos;
layout(location = 2) in mat3 vModelViewOrientationMatrixT;
layout(location = 5) in vec3 vEyeOffset;
layout(location = 6) in vec3 vDirection;

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

void main()
{
    vec4 colourOffsetScaleX=vec4(0.0,0.0,1.0,0.6667);
    vec4 depthOffsetScaleX=vec4(0.0,0.6667,0.5,0.3333);

    vec4 lookup = textureLod(cubemapTexture, vSampleVec,0.0);
    vec3 colourSampleVec=vSampleVec;
    for (int i = 0; i < 3; i++)
    {
        float depth = lookup.a;
        float dist_m=max(0.2,20.0*depth);
        vec3 pos_m=dist_m*vDirection;
        pos_m+=vEyeOffset* step(-0.9, -depth);
        colourSampleVec  = normalize(vec3(-pos_m.z, pos_m.x, pos_m.y));
        lookup=textureLod(cubemapTexture, colourSampleVec, 0.0);
    }
//finalLookup.rgb+=fract(vEyeOffset);
    gl_FragColor = pow(lookup,vec4(.44,.44,.44,1.0));
    //8.0*abs(vSampleVec.z)
}
