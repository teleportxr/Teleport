precision highp float;

const float PI    = 3.1415926536;
const float TwoPI = 2.0 * PI;

uniform vec4 colourOffsetScale;
uniform vec4 depthOffsetScale;
uniform samplerCube cubemapTexture;

layout(location = 0) in vec3 vSampleVec;
layout(location = 1) in vec3 vEyespacePos;
layout(location = 2) in mat3 vModelViewOrientationMatrixT;
layout(location = 5) in float vEyeOffset;

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

    float depth = textureLod(cubemapTexture, vSampleVec,5.0).a;

    // offset angle is atan(4cm/distance)
    // depth received is distance/50metres.

    float dist_m=max(1.0,50.0*depth);
    float angle=(vEyeOffset/dist_m);
    // so distance
    float c=1.0;//cos(angle);
    float s=angle;//sin(angle);

    mat3 OffsetRotationMatrix=mat3(c,0.0,s,0.0,1.0,0.0,-s,0.0,c);
    vec3 worldspace_dir = vModelViewOrientationMatrixT*(OffsetRotationMatrix*vEyespacePos.xyz);
    vec3 colourSampleVec  = normalize(vec3(-worldspace_dir.z, worldspace_dir.y, worldspace_dir.x));
    vec2 uv=WorldspaceDirToUV(colourSampleVec);
    vec3 vSampleVecCorrected=vec3(vSampleVec.x,vSampleVec.z,vSampleVec.y);
    gl_FragColor = pow(textureLod(cubemapTexture, vSampleVecCorrected, 2.0),vec4(.44,.44,.44,1.0));
    //8.0*abs(vSampleVec.z)
}
