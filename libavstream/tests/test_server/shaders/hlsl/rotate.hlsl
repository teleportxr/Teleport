// Copyright (c) 2018 Simul.co

cbuffer RenderConstants : register(b0)
{
	uint id;
	float time;
};

RWTexture2D<float4> output : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 ThreadID : SV_DispatchThreadID)
{
	uint frameWidth, frameHeight;
	output.GetDimensions(frameWidth, frameHeight);
	if(ThreadID.x >= frameWidth || ThreadID.y >= frameHeight) {
		return;
	}

	const float uSpeed = 0.5;
	const float vSpeed = 0.3;

	float theta = time;
	float2x2 rotMatrix = float2x2(float2(cos(theta), -sin(theta)), float2(sin(theta), cos(theta)));

	float2 uv = ThreadID / float2(frameWidth, frameHeight);
	uv = mul(rotMatrix, uv);

	float3 color = float3(frac(uv.x + time * uSpeed), frac(uv.y + time * vSpeed), id ? 1.0 : 0.0);
	output[int2(ThreadID)] = float4(color, 1.0);
}
