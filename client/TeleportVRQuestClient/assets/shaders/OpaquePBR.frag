//#version 310 es
// Precision: mediump or below can produce texture sampling problems for larger objects.
precision highp float;

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
layout(location = 9)  in vec4 v_Color;
layout(location = 10) in vec4 v_Joint;
layout(location = 11) in vec4 v_Weights;
layout(location = 12) in vec3 v_CameraPosition;
struct SurfaceLightProperties
{
	vec3 directionToLight;      // 1 location
	float distanceToLight;
	vec3 halfway;
	float n_h;
	float l_h;
	float n_l;
};
layout(location = 14) in vec3 v_VertexLight_directionToLight[4];   // Consumes 4 locations.
layout(location = 18) in float v_VertexLight_distanceToLight[4];   // Consumes 4 locations.
layout(location = 22) in vec3 v_VertexLight_halfway[4];   // Consumes 4 locations.
layout(location = 26) in vec3 v_VertexLight_nh_lh_nl[4];   // Consumes 4 locations.

#define lerp mix
#define SIMUL_PI_F (3.1415926536)
//From Application SR

//Material
layout(std140, binding = 2) uniform u_MaterialData //Layout conformant to GLSL std140
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
	float range;
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

//From Application SR
layout(std140, binding = 0) uniform u_CameraData
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
	vec4 u_Orientation; //Quaternion
	vec3 u_Position;
	float u_DrawDistance;
} cam;

layout(std430, binding = 1) buffer TagDataCube_ssbo
{
	VideoTagDataCube tagDataCube;
};

layout(binding = 10) uniform sampler2D u_DiffuseTexture;
layout(binding = 11) uniform sampler2D u_NormalTexture;
layout(binding = 12) uniform sampler2D u_CombinedTexture;
layout(binding = 13) uniform sampler2D u_EmissiveTexture;

layout(binding = 14) uniform samplerCube u_SpecularCubemap;
//layout(binding = 15) uniform samplerCube u_LightsCubemap;
//layout(binding = 16) uniform samplerCube u_DiffuseCubemap;

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
	return vec3(-p.z,p.x,p.y);
}

float VisibilityTerm(float roughness2, float l_h)
{
	return 1.0/(l_h*l_h*(1.0-roughness2)+roughness2);
}

float DistributionTerm(float roughness2, float n_h)
{
	float d = (n_h * roughness2 - n_h) * n_h + 1.0;
	return roughness2 / (d * d * SIMUL_PI_F);
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
	return max(0.0,log2(roughness * 1.2) + 3.0);
}

struct SurfaceState
{
	vec3 kS;
	float n_v;
	vec3 kD;
	// pre-sampled environment maps:
	vec3 env;
};

// The instantaneous properties at a specific point on a surface.
struct SurfaceProperties
{
	vec3 albedo;// diffuse color
	float roughness;
	vec3 normal;
	float roughness2;
	vec3 emission;
	float ao;
	vec3 position;
	float metallic;
	vec3 diffuse_env;	// pre-sampled environment map
	float alpha;// alpha for transparencies
};

//SPECULAR
//Fresnel-Schlick
vec3 FresnelSchlick(vec3 specularColour, float v_h)
{
	 return specularColour + (vec3(1.0, 1.0, 1.0)- specularColour) * pow((1.0 - v_h), 5.0);
}

SurfaceState PreprocessSurface(vec3 viewDir, SurfaceProperties surfaceProperties,bool lookupEnv)
{
	SurfaceState surfaceState;
	float roughness_mip		= MipFromRoughness(surfaceProperties.roughness, 5.0);
	// Constant normal incidence Fresnel factor for all dielectrics.
	vec3 Fdielectric		= vec3(0.04, 0.04, 0.04);
	// Base reflectivity F0 at normal incidence (for metals use albedo color).
	vec3 F0					= lerp(Fdielectric, surfaceProperties.albedo, surfaceProperties.metallic);
	// Angle between surface normal and outgoing light direction.
	float cosLo				= saturate(dot(surfaceProperties.normal, -viewDir));
	vec3 refl				= reflect(viewDir, surfaceProperties.normal);
	surfaceState.n_v		= saturate(dot(surfaceProperties.normal, viewDir));
	surfaceState.kS			= FresnelSchlick(F0, cosLo);
	refl					= ConvertCubemapTexcoords(refl.xyz);
	if(lookupEnv)
		surfaceState.env		= textureLod(u_SpecularCubemap, refl, roughness_mip).rgb;
	else
		surfaceState.env=vec3(0,0,0);
	surfaceState.kD			= lerp(vec3(1.0, 1.0, 1.0) - surfaceState.kS, vec3(0.0, 0.0, 0.0), surfaceProperties.metallic);
	return surfaceState;
}

vec3 ZiomaEnvBRDFApprox(vec3 specularColour, float roughness, float n_v)
{
	float mult=1.0-max(roughness,n_v);
	float m3=mult*mult*mult;
	return specularColour+vec3(m3,m3,m3);
}

vec3 PBRAmbient(SurfaceState surfaceState, vec3 viewDir, SurfaceProperties surfaceProperties)
{
	vec3 diffuse			=surfaceState.kD*surfaceProperties.albedo * surfaceProperties.diffuse_env;
	diffuse					*=surfaceProperties.ao;

	vec3 envSpecularColour	=ZiomaEnvBRDFApprox(surfaceProperties.albedo, surfaceProperties.roughness, surfaceState.n_v);
	vec3 specular			=surfaceState.kS*envSpecularColour*surfaceState.env;
	vec3 colour				=diffuse+specular;

	return colour;
}

vec4 Gamma(vec4 a)
{
	return pow(a, vec4(.45, .45, .45, 1.0));
}

vec3 PBRLight(SurfaceState surfaceState, vec3 viewDir,SurfaceLightProperties sl, SurfaceProperties surfaceProperties, vec3 irradiance_n_l)
{
	float lightD					=DistributionTerm(surfaceProperties.roughness2, sl.n_h);
	float lightV					=VisibilityTerm(surfaceProperties.roughness2, sl.l_h);
	// Per-light:
	vec3 diffuse					=surfaceState.kD*irradiance_n_l * surfaceProperties.albedo ;
	vec3 specular					= irradiance_n_l * ( lightD*lightV * surfaceState.kS );
	//float ao						= SceneAO(pos, normal, localToWorld);
	///	specular						*=surfaceState.kS*saturate(pow(surfaceState.n_v + surfaceProperties.ao, surfaceProperties.roughness2) - 1.0 + surfaceProperties.ao);
	vec3 colour = diffuse+specular;

	//colour.r=sl.n_h;
	//colour.g=sl.l_h;
	//colour.b=sl.n_l;
	return colour;//vec3(n_h,n_h,n_h);
}

vec3 SpotLight(SurfaceState surfaceState, vec3 viewDir, SurfaceProperties surfaceProperties, LightTag lightTag)
{
	SurfaceLightProperties sl;
	vec3 diff						=lightTag.position-surfaceProperties.position;
	sl.distanceToLight      		=length(diff);
	float d                         =max(1.0, sl.distanceToLight/lightTag.radius);
	float atten						=step(sl.distanceToLight,lightTag.range);
	vec3 irradiance					=lightTag.colour.rgb*atten/(d*d);
	sl.directionToLight		=normalize(diff);
	sl.halfway				=normalize(-viewDir+sl.directionToLight);
	sl.n_h						=saturate(dot(sl.halfway,surfaceProperties.normal));
	sl.l_h						=saturate(dot(sl.halfway,sl.directionToLight));
	return PBRLight(surfaceState, viewDir,sl,surfaceProperties, irradiance);
}

vec3 DirectionalLight(SurfaceState surfaceState, vec3 viewDir, SurfaceProperties surfaceProperties, LightTag lightTag)
{
	SurfaceLightProperties sl;
	sl.directionToLight    =-lightTag.direction;
	sl.distanceToLight=1.0f;
	sl.halfway				=normalize(-viewDir+sl.directionToLight);
	sl.n_h						=saturate(dot(sl.halfway,surfaceProperties.normal));
	sl.l_h						=saturate(dot(sl.halfway,sl.directionToLight));
	return PBRLight(surfaceState, viewDir, sl,surfaceProperties, lightTag.colour.rgb);
}

vec3 PBRAddLight(SurfaceState surfaceState, vec3 viewDir, SurfaceProperties surfaceProperties, LightTag lightTag)
{
	SurfaceLightProperties sl;
	vec3 diff					=lightTag.position-surfaceProperties.position;
	sl.distanceToLight			=length(diff);
	float d						=max(0.001,sl.distanceToLight/lightTag.radius);
	float atten					=step(sl.distanceToLight,lightTag.range);
	vec3 irradiance				=lightTag.colour.rgb*lerp(1.0,atten/(d*d),lightTag.is_point);
	sl.directionToLight			=lerp(-lightTag.direction,normalize(diff),lightTag.is_point);
	sl.n_l						=saturate(dot(surfaceProperties.normal, sl.directionToLight));
	sl.halfway					=normalize(-viewDir+sl.directionToLight);
	sl.n_h						=saturate(dot(sl.halfway,surfaceProperties.normal));
	sl.l_h						=saturate(dot(sl.halfway,sl.directionToLight));
	return PBRLight(surfaceState, viewDir, sl,surfaceProperties, irradiance*sl.n_l);
}

vec3 PBRAddVertexLight(SurfaceState surfaceState, vec3 viewDir, SurfaceProperties surfaceProperties, LightTag lightTag, SurfaceLightProperties vertexLight)
{
	float d							=max(0.001,vertexLight.distanceToLight/lightTag.radius);
	float atten						=step(vertexLight.distanceToLight,lightTag.range);
	vec3 irradiance					=lightTag.colour.rgb*lerp(1.0,atten/(d*d),lightTag.is_point);
	return PBRLight(surfaceState, viewDir, vertexLight,surfaceProperties, irradiance*vertexLight.n_l);
}

SurfaceProperties GetSurfaceProperties(bool diffuseTex, bool normalTex, bool combinedTex, bool emissiveTex, bool ambient, int maxLights,bool debug)
{
	SurfaceProperties surfaceProperties;
	surfaceProperties.position        =v_Position;
	if (normalTex)
	{
		vec3 normalLookup			=texture(u_NormalTexture, v_UV_normal * u_NormalTexCoordsScalar_R).rgb;
		vec3 tangentSpaceNormalMap	=u_NormalOutputScalar.xyz*2.0*(normalLookup.rgb - vec3(0.5, 0.5, 0.5));// * u_NormalOutputScalar.rgb;
		surfaceProperties.normal	=normalize(v_TBN * tangentSpaceNormalMap);
	}
	else
	{
		surfaceProperties.normal	=v_Normal;
	}
	// Sample the environment maps:
	if(ambient)
		surfaceProperties.diffuse_env	=textureLod(u_SpecularCubemap, ConvertCubemapTexcoords(surfaceProperties.normal.xyz),5.0).rgb;
	else
		surfaceProperties.diffuse_env	=vec3(0,0,0);
	if (combinedTex)
	{
		vec3 roughMetalOcclusion;
		vec4 combinedLookup				=texture(u_CombinedTexture, v_UV_diffuse * u_CombinedTexCoordsScalar_R);
		// from combinedLookup we will either use roughness=A+B*combinedLookup.a;
		roughMetalOcclusion             =u_CombinedOutputScalarRoughMetalOcclusion.rgb*combinedLookup.agb;
		roughMetalOcclusion.r           +=u_CombinedOutputScalarRoughMetalOcclusion.a;
		surfaceProperties.roughness		=roughMetalOcclusion.r;
		surfaceProperties.metallic      =roughMetalOcclusion.g;
		surfaceProperties.ao            =roughMetalOcclusion.b;
	}
	else
	{
		surfaceProperties.roughness		=u_CombinedOutputScalarRoughMetalOcclusion.r;
		surfaceProperties.metallic		=u_CombinedOutputScalarRoughMetalOcclusion.g;
		surfaceProperties.ao			=u_CombinedOutputScalarRoughMetalOcclusion.b;
	}
	if (diffuseTex)
	{
		vec3 diffuseColour			=texture(u_DiffuseTexture, v_UV_diffuse* u_DiffuseTexCoordsScalar_R).rgb;
		surfaceProperties.albedo	=diffuseColour.rgb * u_DiffuseOutputScalar.rgb;
	}
	else
	{
		surfaceProperties.albedo	=u_DiffuseOutputScalar.rgb;
	}
	surfaceProperties.roughness2	=surfaceProperties.roughness*surfaceProperties.roughness;
	return surfaceProperties;
}

void PBR(bool diffuseTex, bool normalTex, bool combinedTex, bool emissiveTex, bool ambient, int maxLights,bool debug)
{
	vec3 diff					=v_Position-v_CameraPosition;
	float dist_to_frag          =length(diff);
	if (dist_to_frag > cam.u_DrawDistance)
		discard;
	vec3 view = normalize(diff);
	SurfaceProperties surfaceProperties=GetSurfaceProperties(diffuseTex,normalTex,combinedTex,emissiveTex,ambient,maxLights,debug);

	SurfaceState surfaceState		=PreprocessSurface(view, surfaceProperties,ambient);
	vec3 c;
	if (ambient)
	{
		c							=PBRAmbient(surfaceState, view, surfaceProperties);
	}
	else
	{
		c							=vec3(0,0,0);
	}

	for (int i=0;i<2;i++)
	{
		//if (i>=tagDataCube.lightCount)
		//	break;
		SurfaceLightProperties vertexLight;
		vertexLight.directionToLight	=v_VertexLight_directionToLight[i];
		vertexLight.distanceToLight		=v_VertexLight_distanceToLight[i];
		vertexLight.halfway				=v_VertexLight_halfway[i];
		vertexLight.n_h					=v_VertexLight_nh_lh_nl[i].x;
		vertexLight.l_h					=v_VertexLight_nh_lh_nl[i].y;
		vertexLight.n_l					=v_VertexLight_nh_lh_nl[i].z;
		//if(view.x>0.0)
			c		+=PBRAddVertexLight(surfaceState, view, surfaceProperties, tagDataCube.lightTags[i],vertexLight);
		//else
		//	c		+=PBRAddLight(surfaceState, view, surfaceProperties, tagDataCube.lightTags[i]);//,vertexLight);

	}
	vec4 u					=vec4(c.rgb, 1.0);
	if (emissiveTex)
	{
		vec3 emissive		= texture(u_EmissiveTexture, v_UV_diffuse * u_EmissiveTexCoordsScalar_R).rgb;
		emissive			*=u_EmissiveOutputScalar.rgb;

		u.rgb				+=emissive.rgb;
	}
	if(debug)
	{
		u.r=surfaceProperties.roughness;
		u.g=surfaceProperties.metallic;
		u.b=surfaceProperties.ao;
	}
	//u.rgb=surfaceProperties.albedo;
	gl_FragColor = Gamma(u);
}

void OpaquePBRDiffuse()
{
	PBR(true, false, false, false, true, 0,false);
}

void OpaquePBRDiffuseNormal()
{
	PBR(true, true, false, false, true, 0,false);
}

void OpaquePBRDiffuseNormalCombined()
{
	PBR(true, true, true, false, true, 0,false);
}

void OpaquePBRLightsOnly()
{
	PBR(true, true, true, false, false, 1,false);
}

void OpaquePBRDebug()
{
	PBR(true, true, true, false, false, 1,true);
}

void OpaquePBR()
{
	PBR(true, true, true, true, true, 1,false);
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
