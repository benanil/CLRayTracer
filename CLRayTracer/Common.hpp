#pragma once
#include <cstdint>
#include <intrin.h>

#if AX_SHARED
#ifdef AX_EXPORT
		#define AX_API __declspec(dllexport)
	#else
		#define AX_API __declspec(dllimport)
	#endif
#else
	#define AX_API
#endif

#ifndef FINLINE
#	ifdef _MSC_VER
#		define FINLINE __forceinline
#   elif __CLANG__
#       define FINLINE [[clang::always_inline]] 
#	elif __GNUC__
#       define FINLINE  __attribute__((always_inline))
#   endif
#endif

#ifndef VECTORCALL
#   ifdef _MSC_VER
#		define VECTORCALL __vectorcall
#   elif __CLANG__
#       define VECTORCALL [[clang::vectorcall]] 
#	elif __GNUC__
#       define VECTORCALL  
#   endif
#endif

#ifndef AXPACK
#	ifdef __GNUC__
#		define AXPACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#	elif _MSC_VER
#		define AXPACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#	endif
#endif

#if defined( __GNUC__ ) || defined(__INTEGRITY)
#	define AX_ALIGNED(_x)          __attribute__ ((aligned(_x)))
#	elif defined( _WIN32) && (_MSC_VER)                                                                                   
#	define AX_ALIGNED(_x)          __declspec(align(_x))      
#	else
	#warning  Need to implement some method to align data here
#	define  CL_ALIGNED(_x)
#endif

#ifndef AXGLOBALCONST
#	if _MSC_VER
#		define AXGLOBALCONST extern const __declspec(selectany)
#	elif defined(__GNUC__) && !defined(__MINGW32__)
#		define AXGLOBALCONST extern const __attribute__((weak))
#   else 
#       define AXGLOBALCONST 
#	endif
#endif

#ifdef _MSC_VER
#	ifndef AXPopCount(x)
#		define AXPopCount(x) __popcnt(x)
#   endif
#	ifndef AXPopCount64(x)
#		define AXPopCount64(x) __popcnt64(x)
#   endif
#elif defined(__GNUC__) && !defined(__MINGW32__)
#	ifndef AXPopCount(x)
#		define AXPopCount(x) __builtin_popcount(x)
#   endif
#	ifndef AXPopCount64(x)
#		define AXPopCount64(x) __builtin_popcountl(x)
#   endif
#else
#	ifndef AXPopCount(x)
#		define AXPopCount(x) PopCount(x)
#   endif
#	ifndef AXPopCount64(x)
#		define AXPopCount64(x) PopCount(x)
#   endif
#endif

#define ENUM_FLAGS(ENUMNAME, ENUMTYPE) \
inline ENUMNAME& operator |= (ENUMNAME& a, ENUMNAME b)          noexcept { return (ENUMNAME&)(((ENUMTYPE&)a) |= ((ENUMTYPE)b)); } \
inline ENUMNAME& operator &= (ENUMNAME& a, ENUMNAME b)			noexcept { return (ENUMNAME&)(((ENUMTYPE&)a) &= ((ENUMTYPE)b)); } \
inline ENUMNAME& operator ^= (ENUMNAME& a, ENUMNAME b)			noexcept { return (ENUMNAME&)(((ENUMTYPE&)a) ^= ((ENUMTYPE)b)); } \
inline constexpr ENUMNAME operator | (ENUMNAME a, ENUMNAME b)	noexcept { return ENUMNAME(((ENUMTYPE)a) | ((ENUMTYPE)b));		} \
inline constexpr ENUMNAME operator & (ENUMNAME a, ENUMNAME b)	noexcept { return ENUMNAME(((ENUMTYPE)a) & ((ENUMTYPE)b));		} \
inline constexpr ENUMNAME operator ~ (ENUMNAME a)				noexcept { return ENUMNAME(~((ENUMTYPE)a));						} \
inline constexpr ENUMNAME operator ^ (ENUMNAME a, ENUMNAME b)	noexcept { return ENUMNAME(((ENUMTYPE)a) ^ (ENUMTYPE)b);		} 

#define ax_assert(...)

typedef unsigned short ushort;
typedef unsigned       uint  ;
typedef unsigned long long ulong ;

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned       uint32;
typedef unsigned long long uint64;

template<typename T>
inline constexpr T PopCount(T i)
{
	if constexpr (sizeof(T) == 4)
	{
		i = i - ((i >> 1) & 0x55555555);        // add pairs of bits
		i = (i & 0x33333333) + ((i >> 2) & 0x33333333);  // quads
		i = (i + (i >> 4)) & 0x0F0F0F0F;        // groups of 8
		return (i * 0x01010101) >> 24;          // horizontal sum of bytes	
	}
	else if constexpr (sizeof(T) == 8) // standard popcount; from wikipedia
	{
		i -= ((i >> 1) & 0x5555555555555555ull);
		i = (i & 0x3333333333333333ull) + (i >> 2 & 0x3333333333333333ull);
		return ((i + (i >> 4)) & 0xf0f0f0f0f0f0f0full) * 0x101010101010101ull >> 56;
	}
	static_assert(1, "not implemented");
}

// maybe we should move this to Algorithms.hpp
template<typename T, typename size_type = ulong>
inline size_type Distance(const T* begin, const T* end)
{
	size_type result;
	while (begin++ < end) result++;
	return result;
}

// we can use these as std::greater, less...
namespace Compare
{
	template<typename T> inline bool Less(T a, T b) { return a < b; }
	template<typename T> inline bool Equal(T a, T b) { return a == b; }
	template<typename T> inline bool NotEqual(T a, T b) { return !Equal(a,b); }
	template<typename T> inline bool Greater(T a, T b) { return !Less(a, b) && !Equal(a, b); }
	template<typename T> inline bool GreaterEqual(T a, T b) { return !Less(a, b); }
	template<typename T> inline bool LessEqual(T a, T b) { return Less(a, b) && Equal(a, b); }
	
	/*for qsort*/ template<typename T>
	inline int QLess(const void* a, const void* b) { return *(T*)a < *(T*)b; }
	/*for qsort*/ template<typename T>
	inline int QGreater(const void* a, const void* b) { return *(T*)a > *(T*)b; }
}