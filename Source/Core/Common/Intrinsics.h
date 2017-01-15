// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Make sure to always include this header before anything else,
// some STL headers include x86intrin.h.

#pragma once

#ifdef _M_X86

#ifdef _MSC_VER
#include <intrin.h>

// MSVC doesn't need this.
#define GNU_TARGET(x)

// These overloads are incomplete, but sufficient for now.
static __m128i operator|(__m128i a, __m128i b)
{
  return _mm_or_si128(a, b);
}

static __m128i operator&(__m128i a, __m128i b)
{
  return _mm_and_si128(a, b);
}

static __m128i operator|=(__m128i& a, __m128i b)
{
  return a = a | b;
}

static __m128i operator&=(__m128i& a, __m128i b)
{
  return a = a & b;
}

static __m256i operator|(__m256i a, __m256i b)
{
  return _mm256_or_si256(a, b);
}

static __m256i operator&(__m256i a, __m256i b)
{
  return _mm256_and_si256(a, b);
}

static __m256i operator|=(__m256i& a, __m256i b)
{
  return a = a | b;
}

static __m256i operator&=(__m256i& a, __m256i b)
{
  return a = a & b;
}

#else  // #ifdef _MSC_VER

#define GNU_TARGET(x) [[gnu::target(x)]]

// Clang 3.7 doesn't define intrinsics that were
// not enabled on the command line, but we can
// work around that.
#ifdef __clang__
#ifndef __SSE3__
#define __SSE3__
#define UNDEF_SSE3
#endif
#ifndef __SSSE3__
#define __SSSE3__
#define UNDEF_SSSE3
#endif
#ifndef __SSE4_1__
#define __SSE4_1__
#define UNDEF_SSE4_1
#endif
#ifndef __SSE4_2__
#define __SSE4_2__
#define UNDEF_SSE4_1
#endif
#ifndef __AVX__
#define __AVX__
#define UNDEF_AVX
#endif
#ifndef __AVX2__
#define __AVX2__
#define UNDEF_AVX2
#endif
#ifndef __BMI__
#define __BMI__
#define UNDEF_BMI
#endif
#ifndef __BMI2__
#define __BMI2__
#define UNDEF_BMI2
#endif
#ifndef __FMA__
#define __FMA__
#define UNDEF_FMA
#endif
#endif

#include <x86intrin.h>

#ifdef UNDEF_SSE3
#undef __SSE3__
#endif
#ifdef UNDEF_SSSE3
#undef __SSSE3__
#endif
#ifdef UNDEF_SSE4_1
#undef __SSE4_1__
#endif
#ifdef UNDEF_SSE4_2
#undef __SSE4_2__
#endif
#ifdef UNDEF_AVX
#undef __AVX__
#endif
#ifdef UNDEF_AVX2
#undef __AVX2__
#endif
#ifdef UNDEF_BMI
#undef __BMI__
#endif
#ifdef UNDEF_BMI2
#undef __BMI2__
#endif
#ifdef UNDEF_FMA
#undef __FMA__
#endif

#endif

#if defined _M_GENERIC
#define _M_SSE 0
#elif _MSC_VER || __INTEL_COMPILER
#define _M_SSE 0x600
#elif defined __GNUC__
#if defined __AVX2__
#define _M_SSE 0x600
#elif defined __AVX__
#define _M_SSE 0x500
#elif defined __SSE4_2__
#define _M_SSE 0x402
#elif defined __SSE4_1__
#define _M_SSE 0x401
#elif defined __SSSE3__
#define _M_SSE 0x301
#elif defined __SSE3__
#define _M_SSE 0x300
#endif
#endif

#endif  // _M_X86
