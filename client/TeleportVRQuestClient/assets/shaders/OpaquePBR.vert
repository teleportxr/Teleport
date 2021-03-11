
//#version 310 es
precision lowp float;

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
layout(std140, binding = 0) uniform u_CameraData
{
    mat4 u_ProjectionMatrix;
    mat4 u_ViewMatrix;
    vec4 u_Orientation; //Quaternion
    vec3 u_Position;
    float _pad;
} cam;

//To Fragment Varying
layout(location = 0)  out vec3 v_Position;
layout(location = 1)  out vec3 v_Normal;
layout(location = 2)  out vec3 v_Tangent;
layout(location = 3)  out vec3 v_Binormal;
layout(location = 4)  out mat3 v_TBN;
layout(location = 7)  out vec2 v_UV_diffuse;
layout(location = 8)  out vec2 v_UV_normal;
layout(location = 9)  out vec4 v_Color;
layout(location = 10) out vec4 v_Joint;
layout(location = 11) out vec4 v_Weights;
layout(location = 12) out vec3 v_CameraPosition;
layout(location = 13) out vec3 v_ModelSpacePosition;

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
    v_Color		        = a_Color;
    v_Joint		        = a_Joint;
    v_Weights	        = a_Weights;
    v_CameraPosition    = cam.u_Position;
    // sm.ViewMatrix[VIEW_ID]._
    //mat4 cob = mat4(vec4(0.0, 1.0, 0.0, 0.0),
    //                vec4(0.0, 0.0, 1.0, 0.0),
    //                vec4(-1.0, 0.0, 0.0, 0.0),
    //                vec4(0.0, 0.0, 0.0, 1.0));
    //v_ModelSpacePosition = (transpose(cob) * vec4(a_Position, 1.0)).xyz;
    v_ModelSpacePosition = a_Position;
}

void Animated()
{
    mat4 boneTransform  = u_Bones[int(a_Joint[0])] * a_Weights[0]
                        + u_Bones[int(a_Joint[1])] * a_Weights[1]
                        + u_Bones[int(a_Joint[2])] * a_Weights[2]
                        + u_Bones[int(a_Joint[3])] * a_Weights[3];

    gl_Position = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * ModelMatrix * (vec4(a_Position, 1.0) * boneTransform);

    v_Position 	        = (ModelMatrix * (vec4(a_Position, 1.0) * boneTransform)).xyz;
    v_Normal	        = normalize((ModelMatrix * (vec4(a_Normal, 0.0) * boneTransform)).xyz);
    v_Tangent	        = normalize((ModelMatrix * (vec4(a_Tangent.xyz, 0.0) * boneTransform)).xyz);

    v_Binormal	        = normalize(cross(v_Normal, v_Tangent));
    v_TBN		        = mat3(v_Tangent, v_Binormal, v_Normal);
    vec2 UV0		    = a_UV0.xy;
    vec2 UV1		    = a_UV1.xy;
    v_UV_diffuse        =(u_DiffuseTexCoordIndex > 0.0 ? UV1 : UV0);
    v_UV_normal         =(u_NormalTexCoordIndex > 0.0 ? UV1 : UV0);
    v_Color		        = a_Color;
    v_Joint		        = a_Joint;
    v_Weights	        = a_Weights;
    v_CameraPosition    = cam.u_Position;
    v_ModelSpacePosition = a_Position.xyz;
}