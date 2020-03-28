

cbuffer MyBuffer : register(b0)
{
	float4 FloatVector;
}


struct VertexInput
{
	float4 position		: POSITION;
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


	// Change the position vector to be 4 units for proper matrix calculations.
	//input.position.w = 1.0f;

	// Calculate the position of the vertex against the world, view, and projection matrices.
	/*output.position = mul(input.position, worldMatrix);
	output.position = mul(output.position, viewMatrix);
	output.position = mul(output.position, projectionMatrix);*/


	output.position = input.position;

	output.uv = input.uv;

	return output;
}

//StructuredBuffer<float4> buffer : register(t0);
Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

float4 ColorPixelShader(VertexOutput input) : SV_TARGET
{
	//int index = input.position.x + input.position.y * 1280;

	//return float4(input.uv, 0.0, 1.0) * buffer[0];
	//return float4(input.uv, 0.0, 1.0) * texture0.Sample(sampler0, input.uv);
	return texture0.Sample(sampler0, float2(input.uv.x, 1.0f - input.uv.y));		// orientation problem with UV or texture loading
	//return float4(input.uv, 0.0, 1.0);
}

float4 ToneMapPS(VertexOutput input) : SV_TARGET
{
	float4 RGBA = texture0.Sample(sampler0, input.uv);
	return float4(pow(RGBA.rgb, 1.0f / 2.2f), RGBA.a);
}


RWBuffer<int4> myBuffer : register(u0);

[numthreads(1, 1, 1)]
void MainComputeShader(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	//myBuffer[0] = texture0.Load(uint3(0, 0, 0)) * 100;// int4(1, 2, 3, 4);
	//myBuffer[0] = int4(1, 2, 3, 4);
	myBuffer[0] = FloatVector;
}


