precision highp float;

layout(location = 0) in vec4 position;

layout(location = 0) out vec3 vSampleVec;
layout(location = 1) out float vDepth;
layout(location = 7) out vec3 vOffsetFromVideo;

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

layout(std430, binding = 0) buffer TagDataCube_ssbo
{
    VideoTagDataCube tagDataCube;
};

vec4 quat_conj(vec4 q)
{
  return vec4(-q.x, -q.y, -q.z, q.w);
}

vec4 quat_inverse(vec4 q)
{
	float len = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
	if (len == 0.0f)
	{
		// Invalid argument
		return vec4(0.0, 0.0, 0.0, 0.0);
	}
	float invLen = 1.0f / len;
	return vec4(-q.x * invLen, -q.y * invLen, -q.z * invLen, q.w * invLen);
}

vec4 quat_mult(vec4 q1, vec4 q2)
{
  vec4 qr;
  qr.w = (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z);
  qr.x = (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y);
  qr.y = (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x);
  qr.z = (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w);
  return qr;
}

vec4 quat_vec(vec4 q, vec3 v)
{
	vec4 qr;
	qr.w =-(q.x * v.x) - (q.y * v.y) - (q.z * v.z);
	qr.x = (q.w * v.x) + (q.y * v.z) - (q.z * v.y);
	qr.y = (q.w * v.y) + (q.z * v.x) - (q.x * v.z);
	qr.z = (q.w * v.z) + (q.x * v.y) - (q.y * v.x);
	return qr;
}

vec3 rotate_by_quaternion(vec4 quat, vec3 pos)
{
  vec4 qr_conj		= quat_conj(quat);

  vec4 q_tmp		= quat_vec(quat, pos);
  vec4 qr			= quat_mult(q_tmp, qr_conj);

  return qr.xyz;
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
    vec3 eyeOffset = vid.eyeOffsets[VIEW_ID].xyz;
    vSampleVec = normalize(position.xyz);
    vSampleVec = normalize(rotate_by_quaternion(quat_inverse(tagDataCube.cameraRotation), vSampleVec));
    vec2 uv = ViewToServerScreenSpace(vSampleVec);
    vec4 lookup = texture(renderTexture, uv);
    vDepth = lookup.a;
    vec4 eye_pos = vec4((sm.ViewMatrix[VIEW_ID] * (vec4(position.xyz, 0.0))).xyz, 1.0);
    vec4 out_pos = sm.ProjectionMatrix[VIEW_ID] * eye_pos;
    vOffsetFromVideo = vid.cameraPosition - tagDataCube.cameraPosition + eyeOffset;
    vOffsetFromVideo = normalize(rotate_by_quaternion(tagDataCube.cameraRotation, vOffsetFromVideo));
    gl_Position = out_pos;
}
