//#version 310 es
precision highp float;

//To Output Framebuffer - Use gl_FragColor
//layout(location = 0) out vec4 colour;

const vec4 inputColour = vec4(1.0, 0.0, 1.0, 1.0);

void main()
{
    gl_FragColor = inputColour;
}