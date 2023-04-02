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

template<typename T> FINLINE constexpr 
bool IsPowerOfTwo(T x) noexcept { return !(x&1) & (x != 0); }

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

FINLINE float RSqrt(float number)
{
	long i;
	float x2, y;
	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;                 // evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );         // what the fuck? 
	y  = * ( float * ) &i;
	y  = y * ( 1.5f - ( x2 * y * y ) );   // 1st iteration
	return y;
}

typedef ushort half;

FINLINE float ConvertHalfToFloat(half x)
{
	const uint e = (x & 0x7C00) >> 10; // exponent
	const uint m = (x & 0x03FF) << 13; // mantissa
	const uint v = BitCast<uint>((float)m) >> 23; // evil log2 bit hack to count leading zeros in denormalized format
	uint a = (x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m);
	a |= ((e == 0) & (m != 0)) * ((v - 37) << 23 | ((m << (150 - v)) & 0x007FE000));
	return BitCast<float>(a); // sign : normalized : denormalized
}

FINLINE float ConvertHalfToFloatSafe(half Value)
{
	uint Mantissa, Exponent, Result;
	Mantissa = (uint)(Value & 0x03FF);

	if ((Value & 0x7C00) != 0)  // The value is normalized
	{
		Exponent = (uint)((Value >> 10) & 0x1F);
	}
	else if (Mantissa != 0)     // The value is denormalized
	{
		// Normalize the value in the resulting float
		Exponent = 1;
		do {
			Exponent--;
			Mantissa <<= 1;
		} while ((Mantissa & 0x0400) == 0);

		Mantissa &= 0x03FF;
	}
	else { // The value is zero
		Exponent = (uint)-112;
	}

	Result = ((Value & 0x8000) << 16) | ((Exponent + 112) << 23) | (Mantissa << 13);
	return *(float*)&Result;
}
	
FINLINE half ConvertFloatToHalf(float Value)
{
	const uint b = BitCast<uint>(Value) + 0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
	const uint e = (b & 0x7F800000) >> 23; // exponent
	const uint m = b & 0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
	uint a = (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13);
	return a | ((e < 113) & (e > 101))*((((0x007FF000 + m) >> (125-e)) + 1) >> 1) | (e > 143) * 0x7FFF; // sign : normalized : denormalized : saturate
}

FINLINE half ConvertFloatToHalfSafe(float Value)
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

	Result = ((IValue + 0x0FFFU + ((IValue >> 13U) & 1U)) >> 13U) & 0x7FFFU; 
	return (half)(Result | Sign);
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

