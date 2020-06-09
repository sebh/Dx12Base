
cbuffer MeshConstantBuffer : register(b0, space1)
{
	float4x4 ViewProjectionMatrix;
	float4x4 ViewProjectionMatrixInv;

	uint	 OutputWidth;
	uint	 OutputHeight;
	uint	 HitShaderClosestHitContribution;
	uint	 HitShaderAnyHitContribution;
}



//
// Ray tracing global root signature parameters in space1
//

RaytracingAccelerationStructure Scene : register(t0, space1);
RWTexture2D<float4> LuminanceRenderTarget : register(u0, space1);

struct RayPayload
{
    float4 Color;
};



[shader("raygeneration")]
void MyRaygenShader()
{
	RayDesc Ray;
	Ray.TMin = 0.0001f;
	Ray.TMax = 1000.0f;
	Ray.Origin = float3(0.0f, 0.0f, 0.0f);
	Ray.Direction = float3(0.0f, 1.0f, 0.0f);

	// Compute clip space
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	float2 DispatchDimension = float2(DispatchRaysDimensions().xy);
	float2 ClipXY = (((LaunchIndex.xy + 0.5f) / DispatchDimension.xy) * 2.0f - 1.0f);
	ClipXY.y *= -1.0f;

	// Compute start and end points from view matrix
	float4 StartPos = mul(ViewProjectionMatrixInv, float4(ClipXY, 0.0f, 1.0));
	StartPos /= StartPos.wwww;
	float4 EndPos = mul(ViewProjectionMatrixInv, float4(ClipXY, 1.0f, 1.0));
	EndPos /= EndPos.wwww;
	Ray.Origin = StartPos.xyz;
	Ray.Direction = normalize(EndPos.xyz - StartPos.xyz);

	// Trace
	RayPayload Payload = { float4(0.5f, 0.5f, 0.5f, 0.0f) };
	uint InstanceInclusionMask = 0xFF;
	// RayFlags: see https://github.com/microsoft/DirectX-Specs/blob/master/d3d/Raytracing.md#ray-flags
#if CLOSESTANDANYHIT==1
	// By default use the closest hit shader
	uint RayFlags = RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
	uint RayContributionToHitGroupIndex = HitShaderClosestHitContribution;
	if (LaunchIndex.y < DispatchDimension.y / 2)
	{
		// If in the top part of the screen, use the any hit shader
		RayFlags = RAY_FLAG_FORCE_NON_OPAQUE; // force the execution of any hit shaders
		RayContributionToHitGroupIndex = HitShaderAnyHitContribution;
	}
	uint MultiplierForGeometryContributionToHitGroupIndex = 0;
	uint MissShaderIndex = 0;
#else
	uint RayFlags = RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
	uint RayContributionToHitGroupIndex = 0;
	uint MultiplierForGeometryContributionToHitGroupIndex = 0;
	uint MissShaderIndex = 0;
#endif

	TraceRay(Scene, RayFlags, InstanceInclusionMask, RayContributionToHitGroupIndex, MultiplierForGeometryContributionToHitGroupIndex, MissShaderIndex, Ray, Payload);

	// Write the raytraced color to the output texture.
	LuminanceRenderTarget[DispatchRaysIndex().xy] = Payload.Color;
}



//
// Ray tracing local root signature parameters in space0
//

struct VertexStruct
{
	float3 Position;
	float3 Normal;
	float2 UV;
};
StructuredBuffer<VertexStruct> VertexBuffer : register(t0, space0);
Buffer<uint> IndexBuffer : register(t1, space0);

#include "StaticSamplers.hlsl"
Texture2D MeshTexture : register(t2, space0);


[shader("closesthit")]
void MyClosestHitShader(inout RayPayload Payload, in BuiltInTriangleIntersectionAttributes Attr)
{
    float3 barycentrics = float3(1 - Attr.barycentrics.x - Attr.barycentrics.y, Attr.barycentrics.x, Attr.barycentrics.y);

	uint vertId = 3 * PrimitiveIndex();
	float2 uv0 = VertexBuffer[IndexBuffer[vertId] + 0].UV;
	float2 uv1 = VertexBuffer[IndexBuffer[vertId] + 1].UV;
	float2 uv2 = VertexBuffer[IndexBuffer[vertId] + 2].UV;
	float2 uv = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

	Payload.Color = float4(MeshTexture.SampleLevel(SamplerLinearClamp, uv, 0).rgb, 1.0);
	//Payload.Color = float4(barycentrics, 1);
	//Payload.Color = float4(WorldRayOrigin() + RayTCurrent() * WorldRayDirection(), 1.0f);
}

[shader("anyhit")]
void MyAnyHitShader(inout RayPayload Payload, in BuiltInTriangleIntersectionAttributes Attr)
{
	float3 barycentrics = float3(1 - Attr.barycentrics.x - Attr.barycentrics.y, Attr.barycentrics.x, Attr.barycentrics.y);
	Payload.Color = float4(barycentrics, 1);
	AcceptHitAndEndSearch();
}

[shader("miss")]
void MyMissShader(inout RayPayload Payload)
{
	Payload.Color = float4(0.1, 0, 0, 1);
}

