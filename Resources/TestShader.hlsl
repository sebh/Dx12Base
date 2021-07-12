


#include "StaticSamplers.hlsl"



cbuffer MyBuffer : register(b0)
{
	float4 FloatVector;
}


struct VertexInput
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORD0;
	uint   VertexID		: SV_VertexID;
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

float4 ColorPixelShader(VertexOutput input) : SV_TARGET
{
	return texture0.Sample(SamplerLinearClamp, float2(input.uv.x, 1.0f - input.uv.y));
}


VertexOutput TriangleVertexShader(VertexInput input)
{
	VertexOutput output;	// TODO init to 0

	const float FloatMul = 100.0f;
	const float4 VecMult = float4(FloatMul, FloatMul, FloatMul, 1.0);

	if (input.VertexID == 0)
		output.position = float4(0.0, 0.5, 0.5, 1.0) * VecMult;
	else if (input.VertexID == 2)
		output.position = float4(0.5, -0.5, 0.5, 1.0) * VecMult;
	else if (input.VertexID == 1)
		output.position = float4(-0.5, -0.5, 0.5, 1.0) * VecMult;

	output.uv = input.uv;

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


