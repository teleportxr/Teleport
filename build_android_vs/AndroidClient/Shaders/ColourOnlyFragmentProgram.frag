#version 450
#extension GL_GOOGLE_cpp_style_line_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

#define GLSL 1
#define SFX 1
#define SFX_TYPED_UAV_LOADS 1
#define SFX_VULKAN 1
#pragma warning(disable:1)
layout(binding = 301) uniform sampler wccSamplerState;
layout(binding = 302) uniform sampler wmcSamplerState;
layout(binding = 303) uniform sampler wrapMirrorSamplerState;
layout(binding = 305) uniform sampler cmcSamplerState;
layout(binding = 306) uniform sampler wrapSamplerState;
layout(binding = 307) uniform sampler wwcSamplerState;
layout(binding = 308) uniform sampler cwcSamplerState;
layout(binding = 309) uniform sampler clampSamplerState;
layout(binding = 310) uniform sampler wrapClampSamplerState;
layout(binding = 311) uniform sampler samplerStateNearest;
layout(binding = 312) uniform sampler wwcNearestSamplerState;
layout(binding = 313) uniform sampler cmcNearestSamplerState;
layout(binding = 314) uniform sampler samplerStateNearestWrap;
layout(binding = 315) uniform sampler samplerStateNearestClamp;
layout(binding = 304) uniform sampler cubeSamplerState;
struct posVertexOutput
{
	vec4 hPosition;
};
//layout(location=0) in gl_PerVertex { vec4 gl_Position; };
layout(location =1) in vec4 hPosition;

layout( location = 0 ) out lowp vec4 outColor;

void main()
{
	//posVertexOutput BlockData0=ioblock.BlockData0;
	vec4 res = vec4(1.0,0,0,0);
	//res.x+=BlockData0.hPosition.x*0.5;
	outColor=res;
}