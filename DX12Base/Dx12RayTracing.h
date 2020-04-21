#pragma once

#include "Dx12Device.h"



class AccelerationStructureBuffer : public RenderResource
{
public:
	AccelerationStructureBuffer(UINT SizeInBytes);
	virtual ~AccelerationStructureBuffer();

private:
	AccelerationStructureBuffer();
	AccelerationStructureBuffer(AccelerationStructureBuffer&);
};



class RayGenerationShader : public ShaderBase
{
public:
	RayGenerationShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~RayGenerationShader();
};

class ClosestHitShader : public ShaderBase
{
public:
	ClosestHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~ClosestHitShader();
};

class AnyHitShader : public ShaderBase
{
public:
	AnyHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~AnyHitShader();
};

class MissShader : public ShaderBase
{
public:
	MissShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros);
	virtual ~MissShader();
};


