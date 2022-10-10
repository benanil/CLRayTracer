#include <stdlib.h>
#include "../Common.hpp"
#include "SIMDCommon.hpp"

namespace Random
{
	// these random seeds slower than PCG and MTwister but good choice for random seed
	FINLINE uint RandomSeed32()
	{
		uint32 result;
		_rdseed32_step(&result);
		return result;
	}

	FINLINE uint64 RandomSeed64()
	{
		uint64 result;
		_rdseed64_step(&result);
		return result;
	}

	class IRandom
	{
	public:
		virtual uint32 Next() = 0;
		virtual uint64 Next64() = 0;

		float NextFloat(float min, float max)
		{
			return min + (NextFloat01() * fabs(min - max));
		}
		float NextFloat01()
		{
			constexpr float c_FMul = (1.0 / 16777216.0f);
			return float(Next() >> 8) * c_FMul;
		}
		double NextDouble01()
		{
			constexpr double c_DMul = (1.0 / 4294967296.0);
			return Next64() * c_DMul;
		}
		double NextDouble(double min, double max)
		{
			return min + (NextDouble01() * fabs(min - max));
		}
		int NextInt(const int _min, const int _max)
		{
			return _min + NextBound(_max - _min);
		}
		uint32 NextUint32(uint32 min, uint32 max)
		{
			return min + NextBound(max - min);
		}
		uint64 NextUint64(uint64 min, uint64 max)
		{
			return min + NextBound(max - min);
		}
		/* returns a random number 0 <= x < bound */
		int NextBound(int bound)
		{
			return Next() % bound;
		}
		uint32 NextBound(uint32 bound)
		{
			return Next() % bound;
		}
		uint64 NextBound(uint64 bound)
		{
			return Next64() % bound;
		}
	};

	// https://www.pcg-random.org/index.html
	// we can also add global state in a cpp file
	// compared to m_MT chace friendly
	struct PCG : public IRandom
	{
		uint64 state = 0x853c49e6748fea9bULL;
		uint64 inc = 0xda3e39cb94b95bdbULL;
	public:
		PCG() {}

		PCG(uint64 seed)
		{
			state = 0x853c49e6748fea9bULL;
			inc = seed << 1 | 1u;
		}

		PCG(uint64 initstate, uint64 seed)
		{
			state = 0U;
			inc = (seed << 1u) | 1u;
			Next();
			state += initstate;
			Next();
		}

		// returns a random number 0 <= x < IntMax
		uint32 Next()
		{
			uint64 oldstate = state;
			// Advance internal state
			state = oldstate * 6364136223846793005ULL + (inc | 1);
			// Calculate output function (XSH RR), uses old state for max ILP
			uint64 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
			uint64 rot = oldstate >> 59u;
			// if you get unary minus error disable sdl checks from msvc settings
			return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
		}

		uint64 Next64()
		{
			uint64 oldstate = state;
			state = oldstate * 6364136223846793005ULL + (inc | 1);
			uint64 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
			uint64 rot = oldstate >> 59u;
			// if you get unary minus error disable sdl checks from msvc settings
			return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
		}
	};


	// Copyright (c) 2011, 2013 Mutsuo Saito, Makoto Matsumoto,
	// Hiroshima University and The University of Tokyo. All rights reserved.
	// generated from paper: http://www.math.sci.hiroshima-u.ac.jp/~m-mat/m_MT/ARTICLES/mt.pdf

	// important: you need to create m_MT classes for each thread
	// 	also I don't recommend using more than one instance in a therad

	class MTwister64 : public IRandom
	{
		static constexpr int N = 624, M = 367;

		uint64 m_MT[N];
		int m_Index = N + 1;

	public:
		// any non zero integer can be used as a seed
		MTwister64(uint64 seed = 4357ul)
		{
			m_MT[0] = seed & ~0ul;
			for (m_Index = 1; m_Index < N; ++m_Index)
				m_MT[m_Index] = (69069 * m_MT[m_Index - 1]) & ~0ul;
		}

		uint32 Next()
		{
			if (m_Index >= N) GenerateNumbers();
			uint64 x = m_MT[m_Index++];
			x ^= x >> 11;
			x ^= x << 7 & 0x9d2c5680ul;
			x ^= x << 15 & 0xefc60000ul;
			x ^= x >> 18;
			return int(x >> 16);
		}

		uint64 Next64()
		{
			if (m_Index >= N) GenerateNumbers();
			uint64 x = m_MT[m_Index++];
			x ^= x >> 11;
			x ^= x << 7 & 0x9d2c5680ul;
			x ^= x << 15 & 0xefc60000ul;
			x ^= x >> 18;
			return x;
		}

	private:

		void GenerateNumbers()
		{
			static const uint64 mag01[2] = { 0x0, 0x9908b0dful };

			int kk = 0;
			uint64 y;

			while (kk < N - M) // unrolled for chace line optimizations
			{
				y = (m_MT[kk] & 0x80000000ul) | (m_MT[kk + 1] & 0x7ffffffful);
				m_MT[kk] = m_MT[kk + M] ^ (y >> 1) ^ mag01[y & 0x1];
				kk++;
				y = (m_MT[kk] & 0x80000000ul) | (m_MT[kk + 1] & 0x7ffffffful);
				m_MT[kk] = m_MT[kk + M] ^ (y >> 1) ^ mag01[y & 0x1];
				kk++;
				y = (m_MT[kk] & 0x80000000ul) | (m_MT[kk + 1] & 0x7ffffffful);
				m_MT[kk] = m_MT[kk + M] ^ (y >> 1) ^ mag01[y & 0x1];
				kk++;
			}
			kk--;

			while (kk < N - 1)
			{
				y = (m_MT[kk] & 0x80000000ul) | (m_MT[kk + 1] & 0x7ffffffful);
				m_MT[kk] = m_MT[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1];
				++kk;
				y = (m_MT[kk] & 0x80000000ul) | (m_MT[kk + 1] & 0x7ffffffful);
				m_MT[kk] = m_MT[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1];
				++kk;
				y = (m_MT[kk] & 0x80000000ul) | (m_MT[kk + 1] & 0x7ffffffful);
				m_MT[kk] = m_MT[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1];
				++kk;
			}

			y = (m_MT[N - 1] & 0x80000000ul) | (m_MT[0] & 0x7ffffffful);
			m_MT[N - 1] = m_MT[M - 1] ^ (y >> 1) ^ mag01[y & 0x1];

			m_Index = 0;
		}
	};

	class MTwister : public IRandom
	{
		static constexpr int SIZE = 624, PERIOD = 397;
		static constexpr int DIFF = SIZE - PERIOD;

		static constexpr uint32 MAGIC = 0x9908b0df;

		uint32 m_MT[SIZE];
		int m_Index = SIZE;

	public:

		// value doesn't matter
		MTwister(uint32 value = 4586u)
		{
			m_MT[0] = value;
			m_Index = SIZE;

			for (uint32 i = 1; i < SIZE; ++i)
				m_MT[i] = 0x6c078965 * (m_MT[i - 1] ^ m_MT[i - 1] >> 30) + i;
		}

		uint32 Next()
		{
			if (m_Index >= SIZE) {
				GenerateNumbers();
				m_Index = 0;
			}
			uint32 y = m_MT[m_Index++];
			y ^= y >> 11u;
			y ^= y << 7u & 0x9d2c5680u;
			y ^= y << 15u & 0xefc60000u;
			y ^= y >> 18u;
			return y;
		}

		// in this case MTwister64 will be more performant
		uint64 Next64()
		{
			if (m_Index + 1 >= SIZE) {
				GenerateNumbers();
				m_Index = 0;
			}
			uint64 y = m_MT[m_Index++];
			uint64 y1 = m_MT[m_Index++];
			y ^= y >> 11ul | (y1 >> 34ul);
			y ^= y1 << 7ul | (y << 39ul) & 0x9d2c5680ul;
			y ^= y << 15ul | (y1 << 47ul) & 0xefc60000ul;
			y ^= y1 >> 18ul | (y >> 50ul);
			return y;
		}

	private:
		void GenerateNumbers()
		{
			size_t i = 0;
			uint32_t y;

			while (i < DIFF) {
				y = (0x80000000 & m_MT[i]) | (0x7FFFFFFF & m_MT[i + 1]);
				m_MT[i] = m_MT[i + PERIOD] ^ (y >> 1) ^ (((int(y) << 31) >> 31) & MAGIC);
				++i;
				y = (0x80000000 & m_MT[i]) | (0x7FFFFFFF & m_MT[i + 1]);
				m_MT[i] = m_MT[i + PERIOD] ^ (y >> 1) ^ (((int(y) << 31) >> 31) & MAGIC);
				++i;
			}

			while (i < SIZE - 1)
			{
				y = (0x80000000 & m_MT[i]) | (0x7FFFFFFF & m_MT[i + 1]);
				m_MT[i] = m_MT[i - DIFF] ^ (y >> 1) ^ (((int(y) << 31) >> 31) & MAGIC);
				++i;
				y = (0x80000000 & m_MT[i]) | (0x7FFFFFFF & m_MT[i + 1]);
				m_MT[i] = m_MT[i - DIFF] ^ (y >> 1) ^ (((int(y) << 31) >> 31) & MAGIC);
				++i;
				y = (0x80000000 & m_MT[i]) | (0x7FFFFFFF & m_MT[i + 1]);
				m_MT[i] = m_MT[i - DIFF] ^ (y >> 1) ^ (((int(y) << 31) >> 31) & MAGIC);
				++i;
			}

			// i = 623, last step rolls over
			y = (0x80000000 & m_MT[SIZE - 1]) | (0x7FFFFFFF & m_MT[0]);
			m_MT[SIZE - 1] = m_MT[PERIOD - 1] ^ (y >> 1) ^ (((int32_t(y) << 31) >> 31) & MAGIC);

			m_Index = 0;
		}
	};

} // namespace Random end