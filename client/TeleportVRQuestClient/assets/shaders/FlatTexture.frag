//#version 310 es
precision highp float;

//To Output Framebuffer - Use gl_FragColor
//layout(location = 0) out vec4 colour;

layout(binding = 10) uniform sampler2D u_Diffuse;
layout(binding = 11) uniform sampler2D u_Normal;
layout(binding = 12) uniform sampler2D u_Combined;

//Layout conformant to GLSL std140
layout(std140, binding = 3) uniform u_MaterialData
{
    vec4 diffuseOutputScalar;
    vec2 diffuseTexCoordsScalar_R;
    vec2 diffuseTexCoordsScalar_G;
    vec2 diffuseTexCoordsScalar_B;
    vec2 diffuseTexCoordsScalar_A;

    vec4 normalOutputScalar;
    vec2 normalTexCoordsScalar_R;
    vec2 normalTexCoordsScalar_G;
    vec2 normalTexCoordsScalar_B;
    vec2 normalTexCoordsScalar_A;

    vec4 combinedOutputScalar;
    vec2 combinedTexCoordsScalar_R;
    vec2 combinedTexCoordsScalar_G;
    vec2 combinedTexCoordsScalar_B;
    vec2 combinedTexCoordsScalar_A;

    vec3 u_SpecularColour;
    float _pad;
} u_MD;

//From Vertex Varying
layout(location = 7)  in vec2 v_UV0;
layout(location = 8)  in vec2 v_UV1;
layout(location = 9)  in vec3 v_CameraPosition;

void main()
{
    vec4 temp = u_MD.diffuseOutputScalar;
    vec4 diffuse = texture(u_Diffuse, v_UV0).bgra;
    vec4 normal = texture(u_Normal, v_UV0).bgra;
    vec4 combined = texture(u_Combined, v_UV0).bgra;
    gl_FragColor = 1.00 * diffuse + 0.01 * normal + 0.01 * combined + 0.01 * temp;
}