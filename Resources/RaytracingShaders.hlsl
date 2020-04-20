
cbuffer MeshConstantBuffer : register(b0, space1)
{
	float4x4 ViewProjectionMatrix;
	float4x4 ViewProjectionMatrixInv;
	uint	 OutputWidth;
	uint	 OutputHeight;
	uint	 pad0;
	uint	 pad1;
}

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

	uint2 LaunchIndex = DispatchRaysIndex().xy;
	float2 DispatchDimension = float2(DispatchRaysDimensions().xy);
	float2 ClipXY = (((LaunchIndex.xy + 0.5f) / DispatchDimension.xy) * 2.0f - 1.0f);

	float4 StartPos = mul(ViewProjectionMatrixInv, float4(ClipXY, 0.0f, 1.0));
	StartPos /= StartPos.wwww;
	float4 EndPos = mul(ViewProjectionMatrixInv, float4(ClipXY, 1.0f, 1.0));
	EndPos /= EndPos.wwww;
	Ray.Origin = StartPos.xyz;
	Ray.Direction = normalize(EndPos.xyz - StartPos.xyz);


	RayPayload Payload = { float4(0.5f, 0.5f, 0.5f, 0.0f) };
	// RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER 
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, Ray, Payload);
	
	// Write the raytraced color to the output texture.
	LuminanceRenderTarget[DispatchRaysIndex().xy] = float4(0.5+0.5*Ray.Direction, 1.0);
	LuminanceRenderTarget[DispatchRaysIndex().xy] = Payload.Color;

}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload Payload, in BuiltInTriangleIntersectionAttributes Attr)
{
    float3 barycentrics = float3(1 - Attr.barycentrics.x - Attr.barycentrics.y, Attr.barycentrics.x, Attr.barycentrics.y);
	Payload.Color = float4(barycentrics, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload Payload)
{
	Payload.Color = float4(0.1, 0, 0, 1);
}

