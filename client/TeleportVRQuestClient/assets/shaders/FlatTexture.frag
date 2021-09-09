precision lowp float;

layout(location = 1) in vec2 vTexCoords;

layout(binding =0) uniform sampler2D u_Texture;

void main()
{
    vec4 lookup = texture(u_Texture, vTexCoords);
    gl_FragColor = pow(lookup, vec4(.44, .44, .44, 1.0));
}
