#pragma once
#include <stdint.h>
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
#		define AXPACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))t
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

#define EXPAND(x) x
#define FOR_EACH_1(what, x, ...) what(x)
#define FOR_EACH_2(what, x, ...) what(x); EXPAND(FOR_EACH_1(what, __VA_ARGS__))
#define FOR_EACH_3(what, x, ...) what(x); EXPAND(FOR_EACH_2(what, __VA_ARGS__))
#define FOR_EACH_4(what, x, ...) what(x); EXPAND(FOR_EACH_3(what, __VA_ARGS__))
#define FOR_EACH_5(what, x, ...) what(x); EXPAND(FOR_EACH_4(what, __VA_ARGS__))
#define FOR_EACH_6(what, x, ...) what(x); EXPAND(FOR_EACH_5(what, __VA_ARGS__))
#define FOR_EACH_7(what, x, ...) what(x); EXPAND(FOR_EACH_6(what, __VA_ARGS__))

#define FOR_EACH_NARG(...) FOR_EACH_NARG_(__VA_ARGS__, FOR_EACH_RSEQ_N())
#define FOR_EACH_NARG_(...) EXPAND(FOR_EACH_ARG_N(__VA_ARGS__))
#define FOR_EACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, N, ...) N
#define FOR_EACH_RSEQ_N() 7, 6, 5, 4, 3, 2, 1, 0

#define GLUE(x, y) x##y
#define FOR_EACH_(N, what, ...) EXPAND(GLUE(FOR_EACH_, N)(what, __VA_ARGS__))
#define FOR_EACH(what, ...) FOR_EACH_(FOR_EACH_NARG(__VA_ARGS__), what, __VA_ARGS__)
#define DELETE_ALL(...) FOR_EACH(delete, __VA_ARGS__)
#define FREE_ALL(...) FOR_EACH(free, __VA_ARGS__)

#define ENUM_FLAGS(ENUMNAME, ENUMTYPE) \
inline ENUMNAME& operator |= (ENUMNAME& a, ENUMNAME b) noexcept { return (ENUMNAME&)(((ENUMTYPE&)a) |= ((ENUMTYPE)b)); } \
inline ENUMNAME& operator &= (ENUMNAME& a, ENUMNAME b) noexcept { return (ENUMNAME&)(((ENUMTYPE&)a) &= ((ENUMTYPE)b)); } \
inline ENUMNAME& operator ^= (ENUMNAME& a, ENUMNAME b) noexcept { return (ENUMNAME&)(((ENUMTYPE&)a) ^= ((ENUMTYPE)b)); } \
inline constexpr ENUMNAME operator | (ENUMNAME a, ENUMNAME b) noexcept { return ENUMNAME(((ENUMTYPE)a) | ((ENUMTYPE)b));		} \
inline constexpr ENUMNAME operator & (ENUMNAME a, ENUMNAME b) noexcept { return ENUMNAME(((ENUMTYPE)a) & ((ENUMTYPE)b));		} \
inline constexpr ENUMNAME operator ~ (ENUMNAME a)			  noexcept { return ENUMNAME(~((ENUMTYPE)a));						} \
inline constexpr ENUMNAME operator ^ (ENUMNAME a, ENUMNAME b) noexcept { return ENUMNAME(((ENUMTYPE)a) ^ (ENUMTYPE)b);		} 

#define ax_assert(...)

template<typename T>
_NODISCARD FINLINE constexpr T PopCount(T x) noexcept
{
#ifdef _MSC_VER
	if      constexpr (sizeof(T) == 4) return __popcnt(x);
	else if constexpr (sizeof(T) == 8) return __popcnt64(x);
#elif defined(__GNUC__) && !defined(__MINGW32__)
	if      constexpr (sizeof(T) == 4) return __builtin_popcount(x);
	else if constexpr (sizeof(T) == 8) return __builtin_popcountl(x);
#else
	if constexpr (sizeof(T) == 4)
	{
		i = i - ((i >> 1) & 0x55555555);        // add pairs of bits
		i = (i & 0x33333333) + ((i >> 2) & 0x33333333);  // quads
		i = (i + (i >> 4)) & 0x0F0F0F0F;        // groups of 8
		return (i * 0x01010101) >> 24;          // horizontal sum of bytes	
	}
	else if (sizeof(T) == 8) // standard popcount; from wikipedia
	{
		i -= ((i >> 1) & 0x5555555555555555ull);
		i = (i & 0x3333333333333333ull) + (i >> 2 & 0x3333333333333333ull);
		return ((i + (i >> 4)) & 0xf0f0f0f0f0f0f0full) * 0x101010101010101ull >> 56;
	}
#endif
}

template<typename T>
_NODISCARD FINLINE constexpr T TrailingZeroCount(T x) noexcept
{
#ifdef _MSC_VER
	if      constexpr (sizeof(T) == 4) return _tzcnt_u32(x);
	else if constexpr (sizeof(T) == 8) return _tzcnt_u64(x);
#elif defined(__GNUC__) && !defined(__MINGW32__)
	if      constexpr (sizeof(T) == 4) return __builtin_ctz(x);
	else if constexpr (sizeof(T) == 8) return __builtin_ctzll(x);
#else
	return PopCount((x & -x) - 1);
#endif
}

template<typename T>
_NODISCARD FINLINE constexpr T LeadingZeroCount(T x) noexcept
{
#ifdef _MSC_VER
	if      constexpr (sizeof(T) == 4) return _lzcnt_u32(x);
	else if constexpr (sizeof(T) == 8) return _lzcnt_u64(x);
#elif defined(__GNUC__) && !defined(__MINGW32__)
	if      constexpr (sizeof(T) == 4) return __builtin_clz(x);
	else if constexpr (sizeof(T) == 8) return __builtin_clzll(x);
#else
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return 32 - PopCount(x);
#endif
}

template<typename To, typename From>
_NODISCARD FINLINE constexpr To BitCast(const From& _Val) noexcept {
	return __builtin_bit_cast(To, _Val);
}

typedef int32_t int32;
typedef int64_t int64;

typedef uint16_t ushort;
typedef uint32_t uint  ;
typedef uint64_t ulong ;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

// maybe we should move this to Algorithms.hpp
template<typename T, typename size_type = ulong>
inline size_type Distance(const T* begin, const T* end)
{
	size_type result = 0ul;
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

enum EForceInit
{
	ForceInit
};

template<typename A, typename B>
struct Pair
{
    A a; 
    B b;

    bool operator==(const Pair& other)
    {
        return a == other.a && b == other.b;
    }

    bool operator!=(const Pair& other)
    {
        return a != other.a || b != other.b;
    }

    Pair(){}
    Pair(A x, B y) : a(x), b(y) {}
};