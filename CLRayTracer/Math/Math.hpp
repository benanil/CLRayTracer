#pragma once
#include "../Common.hpp"
#include <math.h>

// constants
constexpr float PI = 3.14159265f;
constexpr float RadToDeg = 180.0f / PI;
constexpr float DegToRad = PI / 180.0f;
constexpr float OneDivPI = 1.0f / PI;
constexpr float PIDiv2 = PI / 2.0f;
constexpr float TwoPI = PI * 2.0f;

template<typename T> FINLINE T Max(const T a, const T b) { return a > b ? a : b; }
template<typename T> FINLINE T Min(const T a, const T b) { return a < b ? a : b; }
template<typename T> FINLINE T Clamp(const T x, const T a, const T b) { return Max(a, Min(b, x)); }

FINLINE bool IsZero(const float x)	noexcept { return fabsf(x) <= 0.0001f; }
FINLINE bool IsZero(const double x)	noexcept { return fabs(x)  <= 0.00001;  }

FINLINE bool IsEqual(const float  x, const float  y) noexcept { return fabsf(x-y) <= 0.001f; }
FINLINE bool IsEqual(const double x, const double y) noexcept { return fabs (x-y) <= 0.0001; }

template<typename RealT>
FINLINE RealT Repeat(const RealT t, const RealT length) noexcept
{
	return Clamp<RealT>(t - floorf(t / length) * length, 0.0, length);
}

template<typename RealT>
FINLINE RealT Lerp(const RealT from, const RealT to, const RealT t) noexcept
{
	return from + (to - from) * t;
}

template<typename RealT>
FINLINE float LerpAngle(const RealT a, const RealT b, const RealT t) noexcept
{
	float delta = Repeat((b - a), 360);
	if (delta > 180) delta -= 360;
	return a + delta * Clamp(t, 0.0, 1.0);
}

FINLINE float rsqrt(float number)
{
	long i;
	float x2, y;
	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;                       // evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck? 
	y  = * ( float * ) &i;
	y  = y * ( 1.5f - ( x2 * y * y ) );   // 1st iteration
	return y;
}

typedef ushort half;

// https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion/60047308#60047308
FINLINE unsigned short ConvertFloatToHalf(float f) {
	unsigned x = *(unsigned*)&f;
	return ((x >> 16u) & 0x8000u) | (((x & 0x7f800000u) - 0x38000000u) >> 13u) & 0x7c00u | (x >> 13u) & 0x03ffu;
}

FINLINE void ConvertFloat2ToHalf2(half* h, unsigned* v) {
	unsigned x = *v, x1 = v[1];
	h[0] = ((x >> 16u) & 0x8000u) | (((x & 0x7f800000u) - 0x38000000u) >> 13u) & 0x7c00u | (x >> 13u) & 0x03ffu;
	h[1] = ((x1 >> 16u) & 0x8000u) | (((x1 & 0x7f800000u) - 0x38000000u) >> 13u) & 0x7c00u | (x1 >> 13u) & 0x03ffu;
}

FINLINE float ConvertHalfToFloat(half h) {
	return ((h & 0x8000u) << 16u) | (((h & 0x7c00u) + 0x1C000u) << 13u) | ((h & 0x03FFu) << 13u);
}

FINLINE uint PackColorRGBU32(float r, float g, float b) {
	return (uint)(r * 255.0f) | ((uint)(g * 255.0f) << 8) | ((uint)(b * 255.0f) << 16);
}

FINLINE uint PackColorRGBU32(float* c) {
	return (uint)(*c * 255.0f) | ((uint)(c[1] * 255.0f) << 8) | ((uint)(c[2] * 255.0f) << 16);
}

FINLINE uint PackColorRGBAU32(float* c) {
	return (uint)(*c * 255.0f) | ((uint)(c[1] * 255.0f) << 8) | ((uint)(c[2] * 255.0f) << 16) | ((uint)(c[3] * 255.0f) << 24);
}

FINLINE void UnpackColorRGBf(unsigned color, float* colorf)
{
	constexpr float toFloat = 1.0f / 255.0f;
	colorf[0] = float(color >> 0  & 0xFF) * toFloat;
	colorf[1] = float(color >> 8  & 0xFF) * toFloat;
	colorf[2] = float(color >> 16 & 0xFF) * toFloat;
}

FINLINE void UnpackColorRGBAf(unsigned color, float* colorf) {
	constexpr float toFloat = 1.0f / 255.0f;
	colorf[0] = float(color >> 0  & 0xFF) * toFloat;
	colorf[1] = float(color >> 8  & 0xFF) * toFloat;
	colorf[2] = float(color >> 16 & 0xFF) * toFloat;
	colorf[3] = float(color >> 24) * toFloat;
}

FINLINE unsigned short ConvertFloatToHalfSafe(float Value)
{
	uint Result;
	uint IValue = ((uint*)(&Value))[0];
	uint Sign = (IValue & 0x80000000U) >> 16U;
	IValue = IValue & 0x7FFFFFFFU;      // Hack off the sign

	if (IValue > 0x47FFEFFFU) {
		// The number is too large to be represented as a half.  Saturate to infinity.
		Result = 0x7FFFU;
	}
	else if (IValue < 0x38800000U) {
		// The number is too small to be represented as a normalized half.
		// Convert it to a denormalized value.
		uint Shift = 113U - (IValue >> 23U);
		IValue = (0x800000U | (IValue & 0x7FFFFFU)) >> Shift;
	}
	else {
		// Rebias the exponent to represent the value as a normalized half.
		IValue += 0xC8000000U;
	}

	Result = ((IValue + 0x0FFFU + ((IValue >> 13U) & 1U)) >> 13U)&0x7FFFU; 
	return (unsigned short)(Result | Sign);
}

// Code below adapted from DirectX::Math
inline void ScalarSinCos(float* pSin, float* pCos, float  Value) noexcept
{
	float quotient = (1.0f / PI) * Value;

	if (Value >= 0.0f)
		quotient = float(int(quotient + 0.5f));
	else quotient = float(int(quotient - 0.5f));

	float y = Value - TwoPI * quotient;
	// Map y to [-pi/2,pi/2] with sin(y) = sin(Value).
	float sign;
	if (y > PIDiv2) {
		y = PI - y;
		sign = -1.0f;
	}
	else if (y < -PIDiv2) {
		y = -PI - y;
		sign = -1.0f;
	}
	else {
		sign = +1.0f;
	}
	float y2 = y * y;
	// 11-degree minimax approximation
	*pSin = (((((-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f) * y2 + 0.0083333310f) * y2 - 0.16666667f) * y2 + 1.0f) * y;
	// 10-degree minimax approximation
	float p = ((((-2.6051615e-07f * y2 + 2.4760495e-05f) * y2 - 0.0013888378f) * y2 + 0.041666638f) * y2 - 0.5f) * y2 + 1.0f;
	*pCos = sign * p;
}