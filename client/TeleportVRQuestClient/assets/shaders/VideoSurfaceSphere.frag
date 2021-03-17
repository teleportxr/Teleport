
precision highp float;

const float PI    = 3.1415926536;
const float TwoPI = 2.0 * PI;

layout(location = 0) in vec3 vSampleVec;
layout(location = 1) in float vDepth;
layout(location = 7) in vec3 vOffsetFromVideo;

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

vec4 mul(mat4 a,vec4 b)
{
    return a*b;
}

void main()
{
    vec4 lookup = textureLod(cubemapTexture, vSampleVec,0.0);
    vec3 view = vSampleVec;
    vec3 colourSampleVec=vSampleVec;
    for (int i = 0; i < 5; i++)
    {
        float depth = lookup.a;
        float dist_m=25.0*depth+5.0;
        vec3 pos_m=dist_m*vSampleVec;
        pos_m+=vOffsetFromVideo* step(-0.99, -depth);
        // But this does not intersect at depth. We want the vector from the original centre, of
        // original radius to hit point
        float R = dist_m;
        float F = length(vOffsetFromVideo);
        float D = -dot(normalize(vOffsetFromVideo), vSampleVec);
        float b = F * D;
        float c = F * F - R * R;
        float U = -b + sqrt(max(b*b - c,0.0));
        pos_m   += (U - R) * vSampleVec*step(-F,0.0);
        colourSampleVec  = normalize(pos_m);
        lookup=textureLod(cubemapTexture, colourSampleVec, 0.0);

       //lookup.rgb+=vec3(depth,depth,depth);
    }
    //lookup.rgb+=vec3(vDepth,vDepth,vDepth);
	//lookup.b=float(RWTagDataID.x)/31.0;
    gl_FragColor = pow(lookup,vec4(.44,.44,.44,1.0));
}
