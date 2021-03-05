
precision lowp float;

const float PI    = 3.1415926536;
const float TwoPI = 2.0 * PI;

layout(location = 0) in vec3 vSampleVec;
layout(location = 1) in mat4 vInvViewProj;
layout(location = 5) in vec3 vEyeOffset;
//layout(location = 6) in vec3 vDirection;
layout(location = 7) in vec2 vTexCoords;

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

layout(binding = 0) uniform samplerCube cubemapTexture;
layout(std140, binding = 1) uniform videoUB
{
    vec4 eyeOffsets[2];
    mat4 invViewProj[2];
    mat4 viewProj;
    vec3 cameraPosition;
    int _pad2;
} vid;

layout(std430, binding = 0) buffer TagDataCube_ssbo
{
    VideoTagDataCube tagDataCube;
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

vec3 SampleVec(vec3 position)
{
    return normalize(vec3(-position.z,position.x,position.y));
}

void main()
{
    vec4 clip_pos=vec4(-1.0,-1.0,1.0,1.0);
    clip_pos.x+=2.0*vTexCoords.x;
    clip_pos.y+=2.0*vTexCoords.y;

    vec3 view=normalize((vInvViewProj*clip_pos).xyz);

    vec3 colourSampleVec= SampleVec(view);
    vec4 lookup = textureLod(cubemapTexture, colourSampleVec,0.0);
    vec3 offsetFromVideo2=vid.cameraPosition-tagDataCube.cameraPosition+vEyeOffset;
   // vec3 view = vSampleVec;
    for (int i = 0; i < 3; i++)
    {
        float depth = lookup.a;
        float dist_m=5.0*(5.0+20.0*depth);
        vec3 pos_m=dist_m*view;
        pos_m+=offsetFromVideo2* step(-0.8, -depth);

        // But this does not intersect at depth. We want the vector from the original centre, of

        // original radius to hit point
        float R = dist_m;
        float F = length(offsetFromVideo2);
        {
            float D = -dot(normalize(offsetFromVideo2), view);
            float b = F * D;
            float c = F * F - R * R;
            float U = -b + sqrt(b * b - c);
            pos_m += (U - R) * view*step(-F,0.0);

            colourSampleVec  = SampleVec(pos_m);
            lookup=textureLod(cubemapTexture, colourSampleVec, 0.0);
        }
    }
    /* lookup.rgb=fract(view);*/
    //lookup.b=0.0;
//lookup.rgb+=abs(offsetFromVideo2);
	//lookup.b=float(RWTagDataID.x)/31.0;
    gl_FragColor = pow(lookup,vec4(.44,.44,.44,1.0));
}
