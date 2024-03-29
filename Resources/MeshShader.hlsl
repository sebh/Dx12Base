


#include "StaticSamplers.hlsl"



struct MeshVertexInput
{
	float3 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD0;
};

struct MeshVertexOutput
{
	float4 SvPosition	: SV_POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD0;
};

cbuffer MeshConstantBuffer : register(b0)
{
	float4x4 ViewProjectionMatrix;
	float4x4 MeshWorldMatrix;
	float	 VoxelisationVolumeDepth;
	float    pad0[3];
}

Texture2D MeshTexture : register(t0);

MeshVertexOutput MeshVertexShader(MeshVertexInput input)
{
	MeshVertexOutput output;

	output.SvPosition = mul(ViewProjectionMatrix, mul(MeshWorldMatrix, float4(input.Position, 1.0)));

	output.Normal = input.Normal;
	output.UV = input.UV;

	return output;
}

float4 MeshPixelShader(MeshVertexOutput input) : SV_TARGET
{
	//return float4(input.Normal.xyz, 1.0f);
	//return float4(input.UV.xy, 0.0f, 1.0f);
	return MeshTexture.Sample(SamplerLinearClamp, float2(input.UV.xy));
}

RWTexture3D<float4> OutVolumeTexture : register(u0);

void MeshVoxelisationPixelShader(MeshVertexOutput input)
{
	const float4 Data = MeshTexture.Sample(SamplerLinearClamp, float2(input.UV.xy));
	OutVolumeTexture[uint3(input.SvPosition.xy, input.SvPosition.z * VoxelisationVolumeDepth)] = Data;
}


