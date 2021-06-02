precision lowp float;

layout(location = 1) in vec2 vTexCoords;

layout(binding = 0) uniform highp samplerExternalOES videoTexture;

void main()
{
    vec4 lookup = texture(videoTexture, vTexCoords);
    gl_FragColor = pow(lookup, vec4(.44, .44, .44, 1.0));
}
