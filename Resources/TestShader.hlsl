


#include "StaticSamplers.hlsl"



cbuffer MyBuffer : register(b0)
{
	float4 FloatVector;
}


struct VertexInput
{
	uint   VertexID		: SV_VertexID;
	float3 position		: POSITION;
	float2 uv			: TEXCOORD0;
};

struct VertexOutput
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD0;
};

VertexOutput ColorVertexShader(VertexInput input)
{
	VertexOutput output;	// TODO init to 0

	output.position = float4(input.position.xyz, 1.0);
	output.uv = input.uv;

	return output;
}

//StructuredBuffer<float4> buffer : register(t0);
Texture2D texture0 : register(t0);

Texture2D<float4> BindlessTextures[ROOT_BINDLESS_SRV_COUNT] : register(ROOT_BINDLESS_SRV_START_REGISTER);

float4 ColorPixelShader(VertexOutput input) : SV_TARGET
{
	const uint TileCountXY = 8;
	const uint TilePixelSize = 32;
	if (ROOT_BINDLESS_SRV_COUNT==64 && 
		all(input.position.xy >= 0) && all(input.position.xy < TileCountXY * TilePixelSize))
	{
		uint textureIndex = (uint(input.position.y) / TilePixelSize) * TileCountXY + uint(input.position.x) / TilePixelSize;
		return BindlessTextures[NonUniformResourceIndex(textureIndex)].Sample(SamplerLinearRepeat, float2(float2(input.position.xy) / float(TilePixelSize)));
	}

	return texture0.Sample(SamplerLinearClamp, float2(input.uv.x, 1.0f - input.uv.y));
}


VertexOutput TriangleVertexShader(uint VertexID : SV_VertexID)
{
	VertexOutput output;	// TODO init to 0

	output.position = float4(VertexID == 2 ? 3.0 : -1.0, VertexID == 1 ? 3.0 : -1.0, 0.1, 1.0);
	output.uv = (output.position.xy + 1.0) * 0.25;

	return output;
}

float4 TrianglePixelShader(VertexOutput input) : SV_TARGET
{
	return float4(input.uv, 0, 0);
}

float4 ToneMapPS(VertexOutput input) : SV_TARGET
{
	float4 RGBA = texture0.Sample(SamplerPointClamp, input.uv);
	return float4(pow(RGBA.rgb, 1.0f / 2.2f), RGBA.a);
}


RWBuffer<int4> myBuffer : register(u0);

[numthreads(1, 1, 1)]
void MainComputeShader(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	myBuffer[0] = FloatVector;
}


