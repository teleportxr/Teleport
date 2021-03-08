//#version 310 es
precision lowp float;

//To Output Framebuffer - Use gl_FragColor
//layout(location = 0) out vec4 colour;

//From Vertex Varying
layout(location = 0)	in vec3 v_Position;
layout(location = 1)	in vec3 v_Normal;
layout(location = 2)	in vec3 v_Tangent;
layout(location = 3)	in vec3 v_Binormal;
layout(location = 4)	in mat3 v_TBN;
layout(location = 7)	in vec2 v_UV_diffuse;
layout(location = 8)	in vec2 v_UV_normal;
layout(location = 9)	in vec4 v_Color;
layout(location = 10) in vec4 v_Joint;
layout(location = 11) in vec4 v_Weights;
layout(location = 12) in vec3 v_CameraPosition;
layout(location = 13) in vec3 v_ModelSpacePosition;


#define lerp mix
#define SIMUL_PI_F (3.1415926536)
//From Application SR

//Material
layout(std140, binding = 2) uniform u_MaterialData//Layout conformant to GLSL std140
{
	vec4 u_DiffuseOutputScalar;
	vec2 u_DiffuseTexCoordsScalar_R;
	vec2 u_DiffuseTexCoordsScalar_G;
	vec2 u_DiffuseTexCoordsScalar_B;
	vec2 u_DiffuseTexCoordsScalar_A;

	vec4 u_NormalOutputScalar;
	vec2 u_NormalTexCoordsScalar_R;
	vec2 u_NormalTexCoordsScalar_G;
	vec2 u_NormalTexCoordsScalar_B;
	vec2 u_NormalTexCoordsScalar_A;

	vec4 u_CombinedOutputScalarRoughMetalOcclusion;
	vec2 u_CombinedTexCoordsScalar_R;
	vec2 u_CombinedTexCoordsScalar_G;
	vec2 u_CombinedTexCoordsScalar_B;
	vec2 u_CombinedTexCoordsScalar_A;

	vec4 u_EmissiveOutputScalar;
	vec2 u_EmissiveTexCoordsScalar_R;
	vec2 u_EmissiveTexCoordsScalar_G;
	vec2 u_EmissiveTexCoordsScalar_B;
	vec2 u_EmissiveTexCoordsScalar_A;

	vec3 u_SpecularColour;
	float _pad;

	float u_DiffuseTexCoordIndex;
	float u_NormalTexCoordIndex;
	float u_CombinedTexCoordIndex;
	float u_EmissiveTexCoordIndex;
};

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

layout(binding = 10) uniform sampler2D u_DiffuseTexture;
layout(binding = 11) uniform sampler2D u_NormalTexture;
layout(binding = 12) uniform sampler2D u_CombinedTexture;
layout(binding = 13) uniform sampler2D u_EmissiveTexture;

layout(binding = 14) uniform samplerCube u_DiffuseCubemap;
layout(binding = 15) uniform samplerCube u_SpecularCubemap;
layout(binding = 17) uniform samplerCube u_LightsCubemap;

layout(binding = 19) uniform sampler2D u_ShadowMap0;
layout(binding = 20) uniform sampler2D u_ShadowMap1;
layout(binding = 21) uniform sampler2D u_ShadowMap2;
layout(binding = 22) uniform sampler2D u_ShadowMap3;
layout(binding = 23) uniform sampler2D u_ShadowMap4;
layout(binding = 24) uniform sampler2D u_ShadowMap5;
layout(binding = 25) uniform sampler2D u_ShadowMap6;
layout(binding = 26) uniform sampler2D u_ShadowMap7;

//Constants
const float PI = 3.1415926535;

//Helper Functions
float saturate(float _val)
{
	return min(1.0, max(0.0, _val));
}
vec3 saturate(vec3 _val)
{
	return min(vec3(1.0, 1.0, 1.0), max(vec3(0.0, 0.0, 0.0), _val));
}
vec3 ConvertCubemapTexcoords(vec3 p)
{
	return vec3(-p.z, p.x, p.y);
}

float VisibilityTerm(float roughness, float n_v, float n_l)
{
	float m2 = roughness * roughness;
	float visV = n_l * sqrt(n_v * (n_v - n_v * m2) + m2);
	float visL = n_v * sqrt(n_l * (n_l - n_l * m2) + m2);
	return saturate(0.5 / max(visV + visL, 0.00001));
}

float DistributionTerm(float roughness, float n_h)
{
	float m2 = roughness * roughness;
	float d = (n_h * m2 - n_h) * n_h + 1.0;
	return m2 / (d * d * SIMUL_PI_F);
}

float GetRoughness(vec4 combinedLookup)
{
	return combinedLookup.r;
}

float GetMetallic(vec4 combinedLookup)
{
	return combinedLookup.g;
}

float GetAO(vec4 combinedLookup)
{
	return combinedLookup.b;
}

float GetSpecular(vec4 combinedLookup)
{
	return combinedLookup.a;
}

//BRDF Reflection Model
//Diffuse: Burley, OrenNayar, Gotandas
//Specular: Dist.: Blinn, Bechmann, GGXaniso
//Specular: Vis.: Implicit, Neumann, Kelemen, Schlick, Smith, SmithJointAprrox.
//Specular: Fresnel: None, Fresnel.

//DIFFUSE
//Lambert
vec3 Lambertian(vec3 diffuseColour)
{
	return diffuseColour * 1.0 / PI;
}


//SchlickGGX
float GSub(vec3 n, vec3 w, float a, bool directLight)
{
	float k = 0.0;
	if (directLight)
	k= 0.125 * pow((a + 1.0), 2.0);
	else
	k = 0.5 * pow(a, 2.0);

	float NDotV = dot(n, w);
	return NDotV / (NDotV * (1.0 - k)) + k;
}
//Smith
float G(vec3 n, vec3 wi, vec3 wo, float a2, bool directLight)
{
	return GSub(n, wi, a2, directLight) * GSub(n, wo, a2, directLight);
}
//GGX
float D(vec3 n, vec3 h, float a2)
{
	float temp = pow(saturate(dot(n, h)), 2.0) * (a2 - 1.0) + 1.0;
	return a2 / (PI * temp * temp);
}

float MipFromRoughness(float roughness, float CubemapMaxMip)
{
	return (log2(roughness * 1.2) + 3.0);
}

struct SurfaceState
{
	vec3 kS;
	vec3 kD;
	vec3 refl;
	float n_v;
	// pre-sampled environment maps:
	vec3 env;
};

// The instantaneous properties at a specific point on a surface.
struct SurfaceProperties
{
	vec3 albedo;// diffuse color
	vec3 normal;
	vec3 emission;
	vec3 position;
	float roughness;
	float roughness2;
	float roughness_mip;
	float metallic;
	float ao;
	float specular;// specular power in 0..1 range
	float gloss;// specular intensity
	float alpha;// alpha for transparencies
	// pre-sampled environment maps:
	vec3 diffuse_env;
};

//SPECULAR
//Fresnel-Schlick
vec3 FresnelSchlick(vec3 specularColour, float v_h)
{
	 return specularColour + (vec3(1.0, 1.0, 1.0)- specularColour) * pow((1.0 - v_h), 5.0);
}

SurfaceState PreprocessSurface(vec3 viewDir, SurfaceProperties surfaceProperties)
{
	SurfaceState surfaceState;
	// Constant normal incidence Fresnel factor for all dielectrics.
	vec3 Fdielectric		= vec3(0.04, 0.04, 0.04);
	// Base reflectivity F0 at normal incidence (for metals use albedo color).
	vec3 F0					= lerp(Fdielectric, surfaceProperties.albedo, surfaceProperties.metallic);
	// Angle between surface normal and outgoing light direction.
	float cosLo				= saturate(dot(surfaceProperties.normal, -viewDir));
	surfaceState.kS			= FresnelSchlick(F0, cosLo);
	surfaceState.kD			= lerp(vec3(1.0, 1.0, 1.0) - surfaceState.kS, vec3(0.0, 0.0, 0.0), surfaceProperties.metallic);
	surfaceState.refl		= reflect(viewDir, surfaceProperties.normal);
	surfaceState.n_v		= saturate(dot(surfaceProperties.normal, viewDir));
	vec3 refl				=ConvertCubemapTexcoords(surfaceState.refl.xyz);
	surfaceState.env		=textureLod(u_SpecularCubemap, refl, surfaceProperties.roughness_mip).rgb;
	return surfaceState;
}

vec3 KarisEnvBRDFApprox(vec3 specularColour, float roughness, float n_v)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * n_v)) * r.x + r.y;
	vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
	return specularColour * AB.x + AB.y;
}

vec3 ZiomaEnvBRDFApprox(vec3 specularColour, float roughness, float n_v)
{
	float mult=1.0-max(roughness,n_v);
	float m3=mult*mult*mult;
	return specularColour +vec3(m3,m3,m3);
}

vec3 PBRAmbient(SurfaceState surfaceState, vec3 viewDir, SurfaceProperties surfaceProperties)
{
	vec3 diffuse			= surfaceState.kD*surfaceProperties.albedo * surfaceProperties.diffuse_env;
	diffuse					*= surfaceProperties.ao;

	vec3 envSpecularColour	=ZiomaEnvBRDFApprox(surfaceProperties.albedo, surfaceProperties.roughness, surfaceState.n_v);
	vec3 specular			=surfaceState.kS*envSpecularColour * surfaceState.env;
	vec3 colour			 = diffuse+specular;

	return colour;
}

vec4 Gamma(vec4 a)
{
	return pow(a, vec4(.45, .45, .45, 1.0));
}

vec3 PBRAddLight(SurfaceState surfaceState, vec3 viewDir, SurfaceProperties surfaceProperties, LightTag lightTag)
{
	vec3 diff						=lightTag.position-surfaceProperties.position;
	float dist_to_light				=length(diff);
	float d							=max(1.0, dist_to_light/lightTag.radius);
	vec3 irradiance					=lightTag.colour.rgb*lerp(1.0,1.0/(d*d),lightTag.is_point);
	vec3 dir_from_surface_to_light	=lerp(-lightTag.direction, normalize(diff), lightTag.is_point);
	float roughnessL				=min(1.0,max(.01, surfaceProperties.roughness2));
	float n_l						= saturate(dot(surfaceProperties.normal, dir_from_surface_to_light));
	vec3 halfway					=normalize(viewDir+dir_from_surface_to_light);
	vec3 refl						=normalize(reflect(viewDir, surfaceProperties.normal));
	float n_h						=saturate(dot(refl, dir_from_surface_to_light));
	float lightD					=DistributionTerm(roughnessL, n_h);
	float lightV					=VisibilityTerm(roughnessL, surfaceState.n_v, n_l);
	// Per-light:
	vec3 diffuse					=surfaceState.kD*irradiance * surfaceProperties.albedo * saturate(n_l);
	vec3 specular					=irradiance * surfaceState.kS * (lightD * lightV * SIMUL_PI_F);

	//specular = D(n_h,roughness)*G(n_v,n_l,roughness)*F(l_h,specularColour)*n_l/(4.0*n_v*n_l);
	//float ao						= SceneAO(pos, normal, localToWorld);
	specular						*=saturate(pow(surfaceState.n_v + surfaceProperties.ao, surfaceProperties.roughness2) - 1.0 + surfaceProperties.ao);
	vec3 colour						= diffuse+specular;

	return colour;
	//return -lightTag.direction;
}

void PBR(bool diffuseTex, bool normalTex, bool combinedTex, bool emissiveTex, bool doLights)
{
	SurfaceProperties surfaceProperties;
	surfaceProperties.position		=v_Position;
	vec3 diff						= v_Position-v_CameraPosition;
	float dist_to_frag				=length(diff);
	if (dist_to_frag>7.0)
		 discard;
	vec3 view = normalize(diff);
	vec3 normal;
#if 1
	if (normalTex)
	{
		vec3 normalLookup				= texture(u_NormalTexture, v_UV_normal * u_NormalTexCoordsScalar_R).rgb;
		vec3 tangentSpaceNormalMap		= 2.0 * (normalLookup.rgb - vec3(0.5, 0.5, 0.5));// * u_NormalOutputScalar.rgb;
		surfaceProperties.normal		=normalize(v_TBN * tangentSpaceNormalMap);
	}
	else
#endif
	{
		normal=v_Normal;
		surfaceProperties.normal		=v_Normal;
	}
	// Sample the environment maps:
	surfaceProperties.diffuse_env		=textureLod(u_DiffuseCubemap, ConvertCubemapTexcoords(surfaceProperties.normal.xyz), 0.0).rgb;
#if 1
	if (diffuseTex)
	{
		vec3 diffuseColour	=texture(u_DiffuseTexture, v_UV_diffuse * u_DiffuseTexCoordsScalar_R).rgb;
		diffuseColour		=diffuseColour.rgb * u_DiffuseOutputScalar.rgb;
		surfaceProperties.albedo		=diffuseColour;
	}
	else
#endif
	{
		surfaceProperties.albedo		=u_DiffuseOutputScalar.rgb;
	}

#if 1
	// vec4 u				=vec4(diffuseColour.rgb, 1.0);
	//u.r+=float(abs(tagDataCube.lightCount-1));
	//gl_FragColor = Gamma(u);
	if (combinedTex)
	{
		vec4 combinedLookup = texture(u_CombinedTexture, v_UV_diffuse * u_CombinedTexCoordsScalar_R);
		// from combinedLookup we will either use roughness*roughnessTexture, or (1-roughness)*smoothnessTexture. This depends on combinedOutputScalarRoughMetalOcclusion.a.
		combinedLookup		=combinedLookup.araa;
		//combinedLookup.a	=1.0-combinedLookup.a;
		// occlusion to 1.0 for now.
		combinedLookup.b	=1.0;
		// So combinedLookup is now rough-metal-occl-smooth
		vec4 roughMetalOcclusion;
		roughMetalOcclusion.rgb			=u_CombinedOutputScalarRoughMetalOcclusion.rgb*combinedLookup.rgb;
		// smoothness:
		roughMetalOcclusion.a			=(1.0-u_CombinedOutputScalarRoughMetalOcclusion.r)*combinedLookup.a;
		// Either roughness or 1.0-smoothness depending on alpha of scalar.
		surfaceProperties.roughness		=1.0-roughMetalOcclusion.a;//lerp(roughMetalOcclusion.r, 1.0-roughMetalOcclusion.a, u_CombinedOutputScalarRoughMetalOcclusion.a);
		surfaceProperties.metallic		=roughMetalOcclusion.g;
		surfaceProperties.ao			=GetAO(roughMetalOcclusion);
	}
	else
#endif
	{
		surfaceProperties.roughness	=u_CombinedOutputScalarRoughMetalOcclusion.r;
		surfaceProperties.metallic	=u_CombinedOutputScalarRoughMetalOcclusion.g;
		surfaceProperties.ao		=u_CombinedOutputScalarRoughMetalOcclusion.b;
	}
	surfaceProperties.roughness2	=surfaceProperties.roughness*surfaceProperties.roughness;
	surfaceProperties.roughness_mip	= MipFromRoughness(surfaceProperties.roughness, 5.0);

	SurfaceState surfaceState		=PreprocessSurface(view, surfaceProperties);
	vec3 c							=PBRAmbient(surfaceState, view, surfaceProperties);
#if 1
	if (doLights)
	for (int i=0;i<4;i++)
	{
		if (i>=tagDataCube.lightCount)
			break;
		c					+=PBRAddLight(surfaceState, view, surfaceProperties, tagDataCube.lightTags[i]);
	}
#endif
	vec4 u					=vec4(c.rgb, 1.0);
#if 1
	if (emissiveTex)
	{
		vec3 emissive		= texture(u_EmissiveTexture, v_UV_diffuse * u_EmissiveTexCoordsScalar_R).rgb;
		emissive			*= u_EmissiveOutputScalar.rgb;

		u.rgb				+=emissive.rgb;
	}
#endif
	//u.rgb=surfaceState.refl;
	gl_FragColor = Gamma(u);
}

void OpaquePBR()
{
	PBR(true, true, true, false, false);
}
void OpaqueAlbedo()
{
	vec3 diffuseColour = texture(u_DiffuseTexture, v_UV_diffuse * u_DiffuseTexCoordsScalar_R).rgb;
	gl_FragColor = Gamma(vec4(diffuseColour, 1.0));
}

void OpaqueNormal()
{
	vec3 normalLookup = texture(u_NormalTexture, v_UV_normal * u_NormalTexCoordsScalar_R).rgb;
	vec3 tangentSpaceNormalMap = 2.0 * (normalLookup.rgb - vec3(0.5, 0.5, 0.5));
	vec3 normal = normalize(v_TBN * tangentSpaceNormalMap);

	normal.z = -normal.z;//Flip Z for Unity to GL/OVR. Will need a different method for Unreal.
	normal = (normal + vec3(1.0f, 1.0f, 1.0f)) / 2.0f;//Move negative normals into visible range.

	gl_FragColor = Gamma(vec4(normal, 1.0));
}

void OpaqueCombined()
{
	vec4 combinedLookup = texture(u_CombinedTexture, v_UV_diffuse * u_CombinedTexCoordsScalar_R);
	gl_FragColor = Gamma(vec4(combinedLookup.rgb, 1.0));
}
