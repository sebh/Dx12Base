

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
	Ray.Origin = float3(0.0f, 0.0f, 0.0f);
	Ray.Direction = float3(0.0f, 1.0f, 0.0f);
	Ray.TMin = 0.0001f;
	Ray.TMax = 1000.0f;
	RayPayload Payload = { float4(0.0f, 0.0f, 0.0f, 0.0f) };
	// RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER 
	TraceRay(Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, ~0, 0, 1, 0, Ray, Payload);
	
	// Write the raytraced color to the output texture.
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
	Payload.Color = float4(0.3, 0, 0, 1);
}

