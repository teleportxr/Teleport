precision highp float;

layout(location = 0) in vec3 position;

layout(location = 1) out vec2 vTexCoords;

void main()
{
    vec2 vOffsets[4];
    vOffsets[0] = vec2(0, 1); // Bottom Left
    vOffsets[1] = vec2(0, 0); // Top Left
    vOffsets[2] = vec2(1, 0); // Top Right
    vOffsets[3] = vec2(1, 1); // Bottom Right

    vTexCoords = vOffsets[gl_VertexID];

    vec4 p = ModelMatrix * vec4(position, 1.0);
    gl_Position = vec4(p.xyz, 1.0);
}
