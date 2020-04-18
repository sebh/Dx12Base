
#include "Dx12RayTracing.h"



RayGenerationShader::RayGenerationShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
RayGenerationShader::~RayGenerationShader() { }

ClosestHitShader::ClosestHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
ClosestHitShader::~ClosestHitShader() { }

AnyHitShader::AnyHitShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
AnyHitShader::~AnyHitShader() { }

MissShader::MissShader(const TCHAR* filename, const TCHAR* entryFunction, const Macros* macros)
	: ShaderBase(filename, entryFunction, L"lib_6_3", macros)
{
	ReCompileIfNeeded(); // Always compile for now, the library ShaderBytecode will be accessed directly.
}
MissShader::~MissShader() { }


