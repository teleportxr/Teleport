
//#version 310 es
precision lowp float;
#define lerp mix
#ifndef TELEPORT_MAX_LIGHTS
#define TELEPORT_MAX_LIGHTS 4
#endif
//From Application VIA
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Tangent;
layout(location = 3) in vec2 a_UV0;
layout(location = 4) in vec2 a_UV1;
layout(location = 5) in vec4 a_Color;
layout(location = 6) in vec4 a_Joint;
layout(location = 7) in vec4 a_Weights;

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
    vec4 ambientMultipliers;
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

//To Fragment Varying
layout(location = 0)  out vec3 v_Position;
layout(location = 1)  out vec3 v_Normal;
layout(location = 2)  out vec3 v_Tangent;
layout(location = 3)  out vec3 v_Binormal;
layout(location = 4)  out mat3 v_TBN;
layout(location = 7)  out vec2 v_UV_diffuse;
layout(location = 8)  out vec2 v_UV_normal;
layout(location = 9)  out vec2 v_UV_lightmap;
layout(location = 10) out vec4 v_Joint;
layout(location = 11) out vec4 v_Weights;
layout(location = 12) out vec3 v_CameraPosition;
struct SurfaceLightProperties
{
    vec3 directionToLight;      // 1 location
    float distanceToLight;
    vec3 halfway;
    vec3 nh2_lh2_nl;
};
layout(location = 14) out vec3 v_VertexLight_directionToLight[4];   // Consumes 4 locations.
layout(location = 18) out float v_VertexLight_distanceToLight[4];   // Consumes 4 locations.
layout(location = 22) out vec3 v_VertexLight_halfway[4];   // Consumes 4 locations.
layout(location = 26) out vec3 v_VertexLight_nh2_lh2_nl[4];   // Consumes 4 locations.
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

layout(std140, binding = 3) uniform u_BoneData
{
    mat4 u_Bones[64];
};
/*
layout(std140, binding = 5) uniform u_PerMeshInstanceData
{
    vec4 u_LightmapScaleOffset;
} perMeshInstance;
*/
struct VertexSurfaceProperties
{
    vec3 position;
    vec3 normal;
};

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

SurfaceLightProperties GetVertexLight(vec3 viewDir, VertexSurfaceProperties surfaceProperties, LightTag lightTag)
{
    SurfaceLightProperties vertexLight;
    vec3 diff                       =lightTag.position-surfaceProperties.position;
    vertexLight.distanceToLight      =length(diff);
    float d                         =max(0.001, vertexLight.distanceToLight/lightTag.radius);
    float atten                     =step(vertexLight.distanceToLight, lightTag.range);
    vec3 irradiance                 =lightTag.colour.rgb*lerp(1.0, atten/(d*d), lightTag.is_point);
    vertexLight.directionToLight    =lerp(-lightTag.direction, normalize(diff), lightTag.is_point);
    vertexLight.nh2_lh2_nl.z		=saturate(dot(surfaceProperties.normal, vertexLight.directionToLight));
    vertexLight.halfway				=normalize(-viewDir+vertexLight.directionToLight);
    float n_h                       =saturate(dot(vertexLight.halfway,surfaceProperties.normal));
    vertexLight.nh2_lh2_nl.x		=n_h*n_h;
    float l_h                       =saturate(dot(vertexLight.halfway,vertexLight.directionToLight));
    vertexLight.nh2_lh2_nl.y         =l_h*l_h;
    //vertexLight.distribution;
    //vertexLight.visibility;
    return vertexLight;
}

void Static()
{
    v_Position 	        = (ModelMatrix * vec4(a_Position, 1.0)).xyz;
    gl_Position         = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * vec4(v_Position, 1.0);
    v_Normal	        = normalize((ModelMatrix * vec4(a_Normal, 0.0)).xyz);
    v_Tangent	        = normalize((ModelMatrix * vec4(a_Tangent.xyz,0.0)).xyz);
    v_Binormal	        = normalize(cross(v_Normal, v_Tangent));
    v_TBN		        = mat3(v_Tangent, v_Binormal, v_Normal);
    vec2 UV0		    = vec2(a_UV0.x,a_UV0.y);
    vec2 UV1		    = vec2(a_UV1.x,a_UV1.y);
    v_UV_diffuse        =(u_DiffuseTexCoordIndex > 0.0 ? UV1 : UV0);
    v_UV_normal         =(u_NormalTexCoordIndex > 0.0 ? UV1 : UV0);
    v_UV_lightmap       =UV1;//*perMeshInstance.u_LightmapScaleOffset.xy+perMeshInstance.u_LightmapScaleOffset.zw;

    v_Joint		        = a_Joint;
    v_Weights	        = a_Weights;
    v_CameraPosition    = cam.u_Position;

    vec3 diff			=v_Position-v_CameraPosition;
    vec3 viewDir        = normalize(diff);

    VertexSurfaceProperties vertexSurfaceProperties;
    vertexSurfaceProperties.position=v_Position;
    vertexSurfaceProperties.normal=v_Normal;
    for (int i=0;i<TELEPORT_MAX_LIGHTS;i++)
    {
        SurfaceLightProperties vertexLight0=GetVertexLight(viewDir,vertexSurfaceProperties,tagDataCube.lightTags[i]);
        v_VertexLight_directionToLight[i]=vertexLight0.directionToLight;
        v_VertexLight_distanceToLight[i]=vertexLight0.distanceToLight;
        v_VertexLight_halfway[i]            =vertexLight0.halfway;
        v_VertexLight_nh2_lh2_nl[i]           =vec3(vertexLight0.nh2_lh2_nl);
    }
}

void Animated()
{
    mat4 boneTransform  = u_Bones[int(a_Joint[0])] * a_Weights[0]
                        + u_Bones[int(a_Joint[1])] * a_Weights[1]
                        + u_Bones[int(a_Joint[2])] * a_Weights[2]
                        + u_Bones[int(a_Joint[3])] * a_Weights[3];

    gl_Position = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * ModelMatrix * (vec4(a_Position, 1.0) * boneTransform);

    v_Position 	        =(ModelMatrix * (vec4(a_Position, 1.0) * boneTransform)).xyz;
    v_Normal	        =normalize((ModelMatrix * (vec4(a_Normal, 0.0) * boneTransform)).xyz);
    v_Tangent	        =normalize((ModelMatrix * (vec4(a_Tangent.xyz, 0.0) * boneTransform)).xyz);

    v_Binormal	        =normalize(cross(v_Normal, v_Tangent));
    v_TBN		        =mat3(v_Tangent, v_Binormal, v_Normal);
    vec2 UV0		    =a_UV0.xy;
    vec2 UV1		    =a_UV1.xy;
    v_UV_diffuse        =(u_DiffuseTexCoordIndex > 0.0 ? UV1 : UV0);
    v_UV_normal         =(u_NormalTexCoordIndex > 0.0 ? UV1 : UV0);
    v_UV_lightmap       =UV1;//*perMeshInstance.u_LightmapScaleOffset.xy+perMeshInstance.u_LightmapScaleOffset.zw;

    v_Joint		        = a_Joint;
    v_Weights	        = a_Weights;
    v_CameraPosition    = cam.u_Position;

    vec3 diff			=v_Position-v_CameraPosition;
    vec3 viewDir        = normalize(diff);

    VertexSurfaceProperties vertexSurfaceProperties;
    vertexSurfaceProperties.position=v_Position;
    vertexSurfaceProperties.normal=v_Normal;
    for (int i=0;i<TELEPORT_MAX_LIGHTS;i++)
    {
        SurfaceLightProperties vertexLight0=GetVertexLight(viewDir,vertexSurfaceProperties,tagDataCube.lightTags[i]);
        v_VertexLight_directionToLight[i]   =vertexLight0.directionToLight;
        v_VertexLight_distanceToLight[i]    =vertexLight0.distanceToLight;
        v_VertexLight_halfway[i]            =vertexLight0.halfway;
        v_VertexLight_nh2_lh2_nl[i]          =vertexLight0.nh2_lh2_nl;
    }
}