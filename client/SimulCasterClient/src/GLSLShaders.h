// (C) Copyright 2018-2019 Simul Software Ltd

namespace shaders {
// We NEED these extensions to update the texture without uploading it every frame.
    static const char* VideoSurface_OPTIONS = R"(
	#extension GL_OES_EGL_image_external_essl3 : require
)";

    static const char* VideoSurface_VS = R"(
	precision highp float;

    layout(location = 0) in vec4 position;

    layout(location = 0) out vec3 vSampleVec;
    layout(location = 1) out vec3 vEyespacePos;
    layout(location = 2) out mat3 vModelViewOrientationMatrixT;
    layout(location = 5) out float vEyeOffset;

    void main() {

        mat3 ViewOrientationMatrix=mat3(sm.ViewMatrix[VIEW_ID]);
        mat3 ModelOrientationMatrix=mat3(ModelMatrix);
        vModelViewOrientationMatrixT=transpose(ViewOrientationMatrix * ModelOrientationMatrix);

		// Equirect map sampling vector is rotated -90deg on Y axis to match UE4 yaw.
		vSampleVec  = normalize(vec3(-position.z, position.y, position.x));
        vec4 eye_pos= ( sm.ViewMatrix[VIEW_ID] * ( ModelMatrix * position ));
        gl_Position     = (sm.ProjectionMatrix[VIEW_ID] * eye_pos);
        vEyespacePos    =eye_pos.xyz;

		vEyeOffset		=0.08*(float(VIEW_ID)-0.5);
    }
)";

    static const char* VideoSurface_FS = R"(
	precision highp float;

	const float PI    = 3.1415926536;
	const float TwoPI = 2.0 * PI;

	uniform vec4 colourOffsetScale;
	uniform vec4 depthOffsetScale;
    uniform samplerExternalOES videoFrameTexture;

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
		vec2  uv_d=WorldspaceDirToUV(vSampleVec);

		float depth = texture2D(videoFrameTexture, OffsetScale(uv_d,depthOffsetScale)).r;

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
		gl_FragColor = 0.003*depthOffsetScale+0.003*colourOffsetScale+texture2D(videoFrameTexture, OffsetScale(uv,colourOffsetScale));
	}
)";

    static const char* FlatColour_VS = R"(
//#version 310 es
precision highp float;

//From Application VIA
layout(location = 0) in vec3 a_Position;

//From Application SR
/*layout(std140, binding = 0) uniform CameraUB
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
	vec4 u_Orientation; //Quaternion
	vec3 u_Position;
	float _pad;
}cam;

layout(std140, binding = 1) uniform TransformUB
{
	mat4 u_ModelMatrix;
}model;*/

void main()
{
	//gl_Position = cam.u_ProjectionMatrix * cam.u_ViewMatrix * model.u_ModelMatrix * vec4(a_Position, 1.0);
    gl_Position = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * ModelMatrix * vec4(a_Position, 1.0) ;
}
)";
    static const char* FlatColour_FS = R"(
//#version 310 es
precision highp float;

//To Output Framebuffer - Use gl_FragColor
//layout(location = 0) out vec4 colour;

const vec4 inputColour = vec4(1.0, 0.0, 1.0, 1.0);

void main()
{
    gl_FragColor = inputColour;
}

)";

    static const char* FlatTexture_VS = R"(
//#version 310 es
precision highp float;

//From Application VIA
layout(location = 0) in vec3 a_Position;
layout(location = 3) in vec2 a_UV0;
layout(location = 4) in vec2 a_UV1;

//From Application SR
/*layout(std140, binding = 0) uniform CameraUB
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
	vec4 u_Orientation; //Quaternion
	vec3 u_Position;
	float _pad;
}cam;

layout(std140, binding = 1) uniform TransformUB
{
	mat4 u_ModelMatrix;
}model;*/

//To Fragment Varying
layout(location = 7)  out vec2 v_UV0;
layout(location = 8)  out vec2 v_UV1;

void main()
{
	//gl_Position = cam.u_ProjectionMatrix * cam.u_ViewMatrix * model.u_ModelMatrix * vec4(a_Position, 1.0);
    gl_Position = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * ModelMatrix * vec4(a_Position, 1.0);

	v_UV0		= a_UV0;
	v_UV1		= a_UV1;
}
)";
    static const char* FlatTexture_FS = R"(
//#version 310 es
precision highp float;

//To Output Framebuffer - Use gl_FragColor
//layout(location = 0) out vec4 colour;

//layout(binding = 10)  uniform sampler2D u_Texture;

//From Vertex Varying
layout(location = 7)  in vec2 v_UV0;
layout(location = 8)  in vec2 v_UV1;

void main()
{
    //gl_FragColor = texture(u_Texture, v_UV0);
    gl_FragColor = vec4(v_UV0.x, v_UV0.y, 0.0, 1.0);
}
)";

    static const char* BasicPBR_VS = R"(
//#version 310 es
precision highp float;

//From Application VIA
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Tangent;
layout(location = 3) in vec2 a_UV0;
layout(location = 4) in vec2 a_UV1;
layout(location = 5) in vec4 a_Color;
layout(location = 6) in vec4 a_Joint;
layout(location = 7) in vec4 a_Weights;

//From Application SR
layout(std140, binding = 0) uniform CameraUB
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
	vec4 u_Orientation; //Quaternion
	vec3 u_Position;
	float _pad;
}cam;

layout(std140, binding = 1) uniform TransformUB
{
	mat4 u_ModelMatrix;
}model;

//To Fragment Varying
layout(location = 0)  out vec3 v_Position;
layout(location = 1)  out vec3 v_Normal;
layout(location = 2)  out vec3 v_Tangent;
layout(location = 3)  out vec3 v_Binormal;
layout(location = 4)  out mat3 v_TBN;
layout(location = 7)  out vec2 v_UV0;
layout(location = 8)  out vec2 v_UV1;
layout(location = 9)  out vec4 v_Color;
layout(location = 10) out vec4 v_Joint;
layout(location = 11) out vec4 v_Weights;
layout(location = 12) out vec3 v_CameraPosition;

void main()
{
	gl_Position = cam.u_ProjectionMatrix * cam.u_ViewMatrix * model.u_ModelMatrix * vec4(a_Position, 1.0);

	v_Position 	= mat3(model.u_ModelMatrix) * a_Position;
	v_Normal	= normalize(mat3(model.u_ModelMatrix) * a_Normal);
	v_Tangent	= normalize(mat3(model.u_ModelMatrix) * a_Tangent.xyz);
	v_Binormal	= normalize(cross(v_Normal, v_Tangent));
	v_TBN		= mat3(v_Tangent, v_Binormal, v_Normal);
	v_UV0		= a_UV0;
	v_UV1		= a_UV1;
	v_Color		= a_Color;
	v_Joint		= a_Joint;
	v_Weights	= a_Weights;
	v_CameraPosition = cam.u_Position;
}
)";
    static const char* BasicPBR_FS = R"(
//#version 310 es
precision highp float;

//To Output Framebuffer - Use gl_FragColor
//layout(location = 0) out vec4 colour;

//From Vertex Varying
layout(location = 0)  in vec3 v_Position;
layout(location = 1)  in vec3 v_Normal;
layout(location = 2)  in vec3 v_Tangent;
layout(location = 3)  in vec3 v_Binormal;
layout(location = 4)  in mat3 v_TBN;
layout(location = 7)  in vec2 v_UV0;
layout(location = 8)  in vec2 v_UV1;
layout(location = 9)  in vec4 v_Color;
layout(location = 10) in vec4 v_Joint;
layout(location = 11) in vec4 v_Weights;
layout(location = 12) in vec3 v_CameraPosition;

//From Application SR
//Lights
const int MaxLights = 64;
struct Light //Layout conformant to GLSL std140
{
	vec4 u_Colour;
	vec3 u_Position;
	float u_Power;		 //Strength or Power of the light in Watts equilavent to Radiant Flux in Radiometry.
	vec3 u_Direction;
	float u_SpotAngle;
};
layout(std140, binding = 2) uniform LightData
{
	Light[MaxLights] u_Lights;
};

//Material
layout(std140, binding = 3) uniform MaterialData //Layout conformant to GLSL std140
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

	vec4 u_CombinedOutputScalar;
	vec2 u_CombinedTexCoordsScalar_R;
	vec2 u_CombinedTexCoordsScalar_G;
	vec2 u_CombinedTexCoordsScalar_B;
	vec2 u_CombinedTexCoordsScalar_A;

	vec3 u_SpecularColour;
	float _pad;
};
layout(binding = 10) uniform sampler2D u_Diffuse;
layout(binding = 11) uniform sampler2D u_Normal;
layout(binding = 12) uniform sampler2D u_Combined;

vec4 GetDiffuse()
{
	return vec4(
	u_DiffuseOutputScalar.r * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_R).r,
	u_DiffuseOutputScalar.g * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_G).g,
	u_DiffuseOutputScalar.b * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_B).b,
	u_DiffuseOutputScalar.a * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_A).a
	);
}

vec3 GetNormals()
{
	vec3 normalMap = vec3(
	u_NormalOutputScalar.r * texture(u_Normal, v_UV0 * u_NormalTexCoordsScalar_R).r,
	u_NormalOutputScalar.g * texture(u_Normal, v_UV0 * u_NormalTexCoordsScalar_G).g,
	u_NormalOutputScalar.b * texture(u_Normal, v_UV0 * u_NormalTexCoordsScalar_B).b
	);
	normalMap = normalize(v_TBN * (normalMap * 2.0 - 1.0));
	return normalMap;
}

float GetAO()
{
	return u_CombinedOutputScalar.r * texture(u_Combined, v_UV0 * u_CombinedTexCoordsScalar_R).r;
}
float GetRoughness()
{
	return u_CombinedOutputScalar.g * texture(u_Combined, v_UV0 * u_CombinedTexCoordsScalar_G).g;
}
float GetMetallic()
{
	return u_CombinedOutputScalar.b * texture(u_Combined, v_UV0 * u_CombinedTexCoordsScalar_B).b;
}
float GetSpecular()
{
	return u_CombinedOutputScalar.a * texture(u_Combined, v_UV0 * u_CombinedTexCoordsScalar_A).a;
}

//Constants
const float PI = 3.1415926535;

//BRDF Reflection Model to add from UE4:
//Diffuse: Burley, OrenNayar, Gotanda
//Specular: Dist.: Blinn, Bechmann, GGXaniso
//Specular: Vis.: Implicit, Neumann, Kelemen, Schlick, Smith, SmithJointAprrox.
//Specular: Fresnel: None, Fresnel.

//DIFFUSE
//Lambert
vec3 Lambertian(vec3 diffuseColour)
{
	return diffuseColour * 1.0 / PI;
}

//SPECULAR
//Fresnel-Schlick
vec3 F(vec3 R0, vec3 h, vec3 wi)
{
	return R0 + (1.0 - R0) * pow((1.0 - abs(dot(h, wi))), 5.0);
}
//SchlickGGX
float GSub(vec3 n, vec3 w, float a, bool directLight)
{
	float k = 0.0;
	if(directLight)
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
	float temp = pow(abs(dot(n, h)), 2.0) * (a2 - 1.0) + 1.0;
	return a2 / (PI * temp * temp);
}

vec3 BRDF(vec3 N, vec3 Wo, vec3 Wi, vec3 H) //Return a RGB value;
{
	//Calculate basic parameters
	vec3 diffuse = GetDiffuse().rgb;
	float metallic = GetMetallic();
	float a2 = GetRoughness() * GetRoughness();

	//Diffuse
	vec3 Diffuse = Lambertian(diffuse);

	//Specular
	vec3 R0 = mix(diffuse, u_SpecularColour, metallic); //Mix R0 based on metallic look up.
	float D = D(N, H, a2);
	vec3 F = F(R0, H, Wi);
	float G = G(N, Wi, Wo, a2, true);
	vec3 Specular = D * F * G * (1.0 / 4.0 * abs(dot(Wo, N)) * abs(dot(Wi, N)));

	vec3 kS = F;
	vec3 kD = vec3(1.0, 1.0, 1.0) - kS;
	kD *= 1.0 - metallic; //Metallic materials will have no diffuse output.

	return kD * Diffuse + Specular; //kS is already included in the Specular calculations.
}

void main()
{
	vec3 Lo;				//Exitance Radiance from the surface in the direction of the camera.
	vec3 Le = vec3(0.0);	//Emissive Radiance from the surface in the direction of the camera, if any.

	//Primary non-light dependent
	vec3 N = GetNormals();
	vec3 Wo = normalize(-v_Position + v_CameraPosition);

	//Loop over lights to calculate Lo (accumulation of L in the direction Wo)
	for(int i = 0; i < MaxLights; i++)
	{
		if(u_Lights[i].u_Power == 0.0)
			continue;

		//Primary light dependent
		vec3 Wi = normalize(-v_Position + u_Lights[i].u_Position);
		vec3 H = normalize(Wo + Wi);

		//Calucate irradance from the light over the sphere of directions.
		float distanceToLight = length(-v_Position + u_Lights[i].u_Position);
		vec3 SPD = vec3(u_Lights[i].u_Colour.r / 3.0 * u_Lights[i].u_Power,	u_Lights[i].u_Colour.g / 3.0 * u_Lights[i].u_Power, u_Lights[i].u_Colour.b / 3.0 * u_Lights[i].u_Power);
		vec3 irradiance = SPD / (4.0 * PI * pow(distanceToLight, 2.0));

		//Because the radiance is only non-zero in the direction Wi,
		//We can replace radiance with irradinace;
		vec3 radiance = irradiance;
		float cosineFactor = abs(dot(Wi, N));

		Lo += Le + BRDF(N, Wo, Wi, H) * radiance * cosineFactor;
	}
    gl_FragColor = Lo; //Gamma Correction?
}
)";

} // shaders