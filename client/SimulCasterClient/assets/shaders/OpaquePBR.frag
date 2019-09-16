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
layout(std140, binding = 2) uniform u_LightData
{
    Light[MaxLights] u_Lights;
};

//Material
layout(std140, binding = 3) uniform u_MaterialData //Layout conformant to GLSL std140
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
layout(binding = 13) uniform samplerCube u_DiffuseCubemap;
layout(binding = 14) uniform samplerCube u_SpecularCubemap;

//Helper Functions
float saturate(float _val)
{
    return min(1.0, max(0.0, _val));
}

vec3 GetEnvironmentDiffuse(vec3 dir)
{
    return texture(u_DiffuseCubemap, dir).rgb;
}

vec3 GetEnvironmentSpecular(vec3 dir, float lod)
{
    return textureLod(u_DiffuseCubemap, dir, lod).rgb;
}

vec3 EnvironmentBRDFApprox(vec3 specularColour, float roughness, float n_v)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * n_v)) * r.x + r.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
    return specularColour * AB.x + AB.y;
}

vec4 GetDiffuse()
{
    return vec4(
    u_DiffuseOutputScalar.b * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_B).b,
    u_DiffuseOutputScalar.g * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_G).g,
    u_DiffuseOutputScalar.r * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_R).r,
    u_DiffuseOutputScalar.a * texture(u_Diffuse, v_UV0 * u_DiffuseTexCoordsScalar_A).a
    );
}

vec3 GetNormals()
{
    vec3 normalMap = vec3(
    u_NormalOutputScalar.b * texture(u_Normal, v_UV0 * u_NormalTexCoordsScalar_B).b,
    u_NormalOutputScalar.g * texture(u_Normal, v_UV0 * u_NormalTexCoordsScalar_G).g,
    u_NormalOutputScalar.r * texture(u_Normal, v_UV0 * u_NormalTexCoordsScalar_R).r
    );
    normalMap = normalize(v_TBN * (normalMap * 2.0 - 1.0));
    return normalMap;
}

float GetMetallic()
{
    return u_CombinedOutputScalar.r * texture(u_Combined, v_UV0 * u_CombinedTexCoordsScalar_R).r;
}
float GetRoughness()
{
    return u_CombinedOutputScalar.g * texture(u_Combined, v_UV0 * u_CombinedTexCoordsScalar_G).g;
}
float GetAO()
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
    return R0 + (1.0 - R0) * pow((1.0 - saturate(dot(h, wi))), 5.0);
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
    float temp = pow(saturate(dot(n, h)), 2.0) * (a2 - 1.0) + 1.0;
    return a2 / (PI * temp * temp);
}

vec3 BRDF(vec3 N, vec3 Wo, vec3 Wi, vec3 H, vec3 lightColour) //Return a RGB value;
{
    vec3 Diffuse	= vec3(0,0,0);
    vec3 Specular	= vec3(0,0,0);

    //Calculate basic parameters
    float roughnessE = GetRoughness() * GetRoughness();
    float roughnessL = max(0.01, roughnessE);
    vec3 R = reflect(Wo, N);

    //Environment Light Calculation
    vec3 environment = mix(GetEnvironmentSpecular(R, roughnessE * 11.0), GetEnvironmentDiffuse(R), saturate((roughnessE - 0.25) / 0.75));

    //Diffuse
    Diffuse += GetDiffuse().rgb * GetEnvironmentDiffuse(R);
    Diffuse += lightColour * GetDiffuse().rgb * saturate(dot(N, Wi));

    //Specular
    vec3 environmentSpecularColour = EnvironmentBRDFApprox(u_SpecularColour, roughnessE, saturate(dot(N, Wo)));
    Specular += environmentSpecularColour * environment;

    float metallic = GetMetallic();
    vec3 R0 = mix(Diffuse, u_SpecularColour, metallic); //Mix R0 based on metallic look up.
    float D = D(N, H, roughnessL);
    vec3 F = F(R0, H, Wi);
    float G = G(N, Wi, Wo, roughnessL, true);
    Specular = lightColour * D * F * G * (1.0 / 4.0 * saturate(dot(Wo, N)) * saturate(dot(Wi, N)));

    //Ambient Occlusion
    float ao = GetAO();
    Specular *= saturate(pow(dot(N, Wo) + ao, roughnessE) - 1.0 + ao);

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

        Lo += Le + BRDF(N, Wo, Wi, H, radiance);
    }
    //gl_FragColor = vec4(Lo, 1.0); //Gamma Correction?
    gl_FragColor = GetDiffuse() + 0.01 * vec4(GetNormals(), 1) + 0.01 * vec4(GetMetallic(), 0, 0, 1) + 0.01 * vec4(GetEnvironmentDiffuse(Wo), 1) + 0.01 * vec4(GetEnvironmentSpecular(Wo, 0.0), 1);
}