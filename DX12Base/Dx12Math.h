#pragma once

#include "DirectXMath.h"
using namespace DirectX;



typedef signed   char s8;
typedef unsigned char u8;
//typedef unsigned char byte;

typedef unsigned int uint;
typedef int          sint;

typedef UINT64 uint64;
typedef INT64  sint64;

typedef short word;
typedef uint dword;

typedef XMMATRIX float4x4;
typedef XMVECTOR float4;



const float Pi = 3.14159265359f;



template<typename T>
inline T Clamp(T x, T MinX, T MaxX)
{ 
	return x > MaxX ? MaxX : (x < MinX ? MinX : x);
}

template<typename T>
inline T Sqr(T x) { return x * x; }

inline float Saturate(float x) 
{
	return x > 1.0f ? 1.0f : (x < 0.0f ? 0.0f : x);
}

inline float Lerp(float a, float b, float x) 
{ 
	return a + Saturate(x) * (b - a);
}

inline float DegToRad(float deg)
{
	return deg / 180.0f * Pi;
}

inline float RadToDeg(float rad)
{
	return rad * 180.0f / Pi;
}

inline uint RoundUp(uint Value, uint Alignement)
{
	uint Var = Value + Alignement - 1;
	return Alignement * (Var / Alignement);
}

inline uint64 RoundUp(uint64 Value, uint64 Alignement)
{
	uint64 Var = Value + Alignement - 1;
	return Alignement * (Var / Alignement);
}


