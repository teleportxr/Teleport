
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

layout(binding = 0) uniform sampler2D renderTexture;
layout(std140, binding = 1) uniform videoUB
{
    vec4 eyeOffsets[2];
    mat4 invViewProj[2];
    mat4 viewProj;
    mat4 serverProj;
    vec3 cameraPosition;
    int _pad2;
    vec4 cameraRotation;
} vid;

layout(std430, binding = 0) buffer TagDataCube_ssbo
{
    VideoTagDataCube tagDataCube;
};

vec4 mul(mat4 a,vec4 b)
{
    return a*b;
}

vec2 ViewToServerScreenSpace(vec3 pos)
{
	vec4 clip_pos = vec4(pos, 1.0);
	clip_pos = vid.serverProj * clip_pos;

	pos = clip_pos.xyz;

	// Move to NDC space
	if (clip_pos.w != 0.0)
	{
		pos /= clip_pos.w;
	}

	// Finally, convert to uv
	// y is flipped because render texure is in HLSL form
	return 0.5 * (vec2(1.0, 1.0) + vec2(pos.x, -pos.y));
}

void main()
{
    //vec4 lookup = vec4(0.0, 0.0, 0.0, vDepth);
    vec3 view = vSampleVec;
    vec2 uv = ViewToServerScreenSpace(view);
    vec4 lookup = texture(renderTexture, uv);
    //for (int i = 0; i < 5; i++)
    //{
	//	float depth = lookup.a;
	//	float dist_m = 25.0 * depth + 5.0;
	//	vec3 pos_m = dist_m * vSampleVec;
	//	pos_m += vOffsetFromVideo * step(-0.99, -depth);
		// But this does not intersect at depth. We want the vector from the original centre, of
		// original radius to hit point
	//	float R = dist_m;
	//	float F = length(vOffsetFromVideo);
	//	float D = -dot(normalize(vOffsetFromVideo), vSampleVec);
	//	float b = F * D;
	//	float c = F * F - R * R;
	//	float U = -b + sqrt(max(b * b - c, 0.0));
	//	pos_m += (U - R) * vSampleVec * step(-F, 0.0);
	//	view = normalize(pos_m);
	//	vec2 uv = ViewToServerScreenSpace(view);
	//	lookup = texture(renderTexture, uv);
    //}
    gl_FragColor = pow(lookup, vec4(.44, .44, .44, 1.0));
}
