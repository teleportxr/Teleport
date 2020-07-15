// Copyright (c) 2018 Simul.co

cbuffer RenderConstants : register(b0)
{
	uint id;
	float time;
};

RWTexture2D<float4> output : register(u0);

float hash(float2 seed)
{
	return frac(sin(dot(seed.xy, float2(12.9898, 78.233))) * 43758.5453);
}

[numthreads(8, 8, 1)]
void main(uint2 ThreadID : SV_DispatchThreadID)
{
	uint frameWidth, frameHeight;
	output.GetDimensions(frameWidth, frameHeight);
	if(ThreadID.x >= frameWidth || ThreadID.y >= frameHeight) {
		return;
	}

	float2 uv   = ThreadID / float2(frameWidth, frameHeight);
	float noise = hash(uv + frac(time));
	float3 hue  = id ? float3(1.0, 0.6, 0.6) : float3(0.6, 0.6, 1.0);

	output[int2(ThreadID)] = float4(noise * hue, 1.0);
}
