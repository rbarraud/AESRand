// AESRand.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <intrin.h>
#include <windows.h>
#include <array>

__m128i state = _mm_setzero_si128();

// AES will only use this key as a simple XOR
__m128i key = _mm_set_epi64x(0xa5a5a5a5a5a5a5a5ll, 0x5a5a5a5a5a5a5a5all);

// The first 7 prime numbers + 1 on the end
__m128i increment128cycle = _mm_set_epi64x(0, 0x110D0B0705030201ll);

// The first 15 prime numbers + 1 on the end
__m128i increment64cycle = _mm_set_epi64x(0x2f2b29251f1d1713, 0x110D0B0705030201ll);
__m128i carryAdd = _mm_set_epi64x(1, 0);

void AESRand_increment(__m128i& state) {
#if 0
	__m128i mask = _mm_cmpeq_epi64(state, _mm_setzero_si128());
	mask = _mm_slli_si128(mask, 8);

	// Top 64-bits get "1" added IF bottom 64-bits equals zero. 
	// Otherwise, top-64 bits get "0" added to them. That's the point
	// of the "blendv" instruction.
	__m128i mixedIncrement = _mm_blendv_epi8(increment128cycle, carryAdd, mask);
#else
	// 64-bit version runs twice as fast: 28GBps instead of 12.9GBps due to cutting out the dependency
	__m128i mixedIncrement = increment64cycle;
#endif
	state = _mm_add_epi64(state, mixedIncrement);
}

/*
__m128i AESRand_randA(const __m128i state) {
	__m128i left = _mm_aesenc_si128(state, key);
	__m128i right = _mm_aesdec_si128(_mm_add_epi64(state, key), _mm_setzero_si128());
	return _mm_xor_si128(left, right);
}

__m128i AESRand_randB(const __m128i state) {
	__m128i left = _mm_aesenc_si128(_mm_xor_si128(state, key), _mm_setzero_si128());
	__m128i right = _mm_aesdec_si128(state, key);
	return _mm_sub_epi64(left, right);
}*/

__m128i AESRand_randA(const __m128i state) {
	return _mm_aesenc_si128(_mm_aesenc_si128(state, increment64cycle), increment64cycle);
}

__m128i AESRand_randB(const __m128i state) {
	return _mm_aesdec_si128(_mm_aesdec_si128(state, increment64cycle), increment64cycle);
}

/*
__m128i AESRand_randA(const __m128i state) {
	return _mm_aesenc_si128(state, key);
}

__m128i AESRand_randB(const __m128i state) {
	return _mm_aesdec_si128(state, key);
}*/

struct m256 {
	__m128i high;
	__m128i low;
};

void AESRand128_inc(m256& state) {
	__m128i negative_carry = _mm_cmpeq_epi64(state.low, _mm_setzero_si128());
	state.high = _mm_sub_epi64(state.high, increment64cycle);
	state.low = _mm_add_epi64(state.low, increment64cycle); 
	state.high = _mm_sub_epi64(state.high, negative_carry);
}

std::array<__m128i, 4> AESRand128_rand(m256& state) {
	__m128i left = _mm_unpackhi_epi64(state.low, state.high);
	__m128i right = _mm_unpacklo_epi64(state.low, state.high);

	// 34.6 GBps
	//return { _mm_aesenc_si128(left, increment64cycle), _mm_aesenc_si128(right, increment64cycle), _mm_aesdec_si128(left, increment64cycle), _mm_aesdec_si128(right, increment64cycle) };

	// 27.0 GBps
	return { _mm_aesenc_si128(_mm_aesenc_si128(left, increment64cycle), increment64cycle),
				_mm_aesenc_si128(_mm_aesenc_si128(right, increment64cycle), increment64cycle),
				_mm_aesdec_si128(_mm_aesdec_si128(left, increment64cycle), increment64cycle),
				_mm_aesdec_si128(_mm_aesdec_si128(right, increment64cycle), increment64cycle) };
}

void timerTest() {
	LARGE_INTEGER start, end, frequency;
	QueryPerformanceFrequency(&frequency);
	__m128i total = _mm_setzero_si128();
	__m128i total2 = _mm_setzero_si128();
	__m128i total3 = _mm_setzero_si128();
	__m128i total4 = _mm_setzero_si128();

	__m128i state[2] = { _mm_setzero_si128(), _mm_set_epi64x(0xDEADBEEFF00DCAFEll, 0x31415926F00DD00D)};
	m256 state2 = { _mm_setzero_si128() , _mm_setzero_si128() };

	QueryPerformanceCounter(&start);

	
	for (long long i = 0; i < 5000000000; i++) {
		AESRand_increment(state[0]);
		//AESRand_increment(state[1]);
		total = _mm_add_epi64(total, AESRand_randA(state[0]));
		total2 = _mm_add_epi64(total2, AESRand_randB(state[0]));
		//total3 = _mm_add_epi64(total3, AESRand_randA(state[1]));
		//total4 = _mm_add_epi64(total4, AESRand_randB(state[1]));
	}
	
	/*
	for (long long i = 0; i < 5000000000; i++) {
		AESRand128_inc(state2);
		auto stuff = AESRand128_rand(state2);
		total = _mm_add_epi64(stuff[0], total);
		total2 = _mm_add_epi64(stuff[1], total2);
		total3 = _mm_add_epi64(stuff[2], total3);
		total4 = _mm_add_epi64(stuff[3], total4);
	}*/

	total = _mm_add_epi64(total, total2);
	total = _mm_add_epi64(total, total3);
	total = _mm_add_epi64(total, total4);

	QueryPerformanceCounter(&end);

	double time = (end.QuadPart - start.QuadPart)*1.0 / frequency.QuadPart;

	std::cout << "Time: " << time << std::endl;
	std::cout << "GBps: " << (5000000000.0 * (32)) / (1 << 30) / time << std::endl;
	std::cout << "Total: " << total.m128i_u64[0] << std::endl;
}

#include <io.h>  
#include <fcntl.h>  
#include <stdio.h>

int main()
{
	timerTest();
	return 0; 
	__m128i output[16384];
	m256 state2 = { _mm_setzero_si128() , _mm_setzero_si128() };

	_setmode(_fileno(stdout), _O_BINARY);

	while (!ferror(stdout)) {
		for (int i = 0; i < 16384; i += 4) {
			AESRand128_inc(state2);
			auto stuff = AESRand128_rand(state2);
			output[i] = stuff[0];
			output[i + 1] = stuff[1];
			output[i + 2] = stuff[2];
			output[i + 3] = stuff[3];
		}

		fwrite(output, 1, sizeof(output), stdout);
	}


}