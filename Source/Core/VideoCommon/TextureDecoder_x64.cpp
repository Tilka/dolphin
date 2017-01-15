// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Intrinsics.h"  // NOLINT

#include <algorithm>
#include <cstring>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureDecoder_Internal.h"

// Silence AVX warning. We use VZEROUPPER, so it should be fine.
// TODO: Check if MSVC uses VEX-encoded XMM stores after extracting from YMM.
#ifdef _MSC_VER
#pragma warning(disable : 4752)
#endif

namespace TextureDecoder
{
// Need to define wrapper functions with appropriate target attributes
// to enable function inlining.

template <int block_width, int block_height, int bytes_per_block_line,
          BlockLineDecoder* decode_block_line, int block_lines_per_call = 1>
GNU_TARGET("ssse3")
static void DecodeTextureSSSE3(u32* __restrict dst, const u8* src, const u16* tlut, int width,
                               int height)
{
  DecodeTexture<block_width, block_height, bytes_per_block_line, decode_block_line,
                block_lines_per_call>(dst, src, tlut, width, height);
}

template <int block_width, int block_height, int bytes_per_block_line,
          BlockLineDecoder* decode_block_line, int block_lines_per_call = 1>
GNU_TARGET("sse4.1")
static void DecodeTextureSSE41(u32* __restrict dst, const u8* src, const u16* tlut, int width,
                               int height)
{
  DecodeTexture<block_width, block_height, bytes_per_block_line, decode_block_line,
                block_lines_per_call>(dst, src, tlut, width, height);
}

template <int block_width, int block_height, int bytes_per_block_line,
          BlockLineDecoder* decode_block_line, int block_lines_per_call = 1>
GNU_TARGET("avx2")
static void DecodeTextureAVX2(u32* __restrict dst, const u8* src, const u16* tlut, int width,
                              int height)
{
  DecodeTexture<block_width, block_height, bytes_per_block_line, decode_block_line,
                block_lines_per_call>(dst, src, tlut, width, height);
  _mm256_zeroupper();
}

static void DecodeBytes_I4_SSE2(u32* dst, const u8* src, const u16*, int width)
{
  __m128i r0, r1, r2, r3, r4;
  r0 = _mm_load_si128((__m128i*)src);

  // Duplicate nibbles.
  r1 = r0 & _mm_set1_epi8((s8)0xF0);
  r2 = r0 & _mm_set1_epi8(0x0F);
  r1 |= _mm_srli_epi16(r1, 4);
  r2 |= _mm_slli_epi16(r2, 4);
  r3 = _mm_unpacklo_epi8(r1, r2);
  r4 = _mm_unpackhi_epi8(r1, r2);

  // Duplicate bytes.
  __m128i w[4];
  w[0] = _mm_unpacklo_epi8(r3, r3);
  w[1] = _mm_unpackhi_epi8(r3, r3);
  w[2] = _mm_unpacklo_epi8(r4, r4);
  w[3] = _mm_unpackhi_epi8(r4, r4);

  // Duplicate words.
  __m128i o[8];
  o[0] = _mm_unpacklo_epi16(w[0], w[0]);
  o[1] = _mm_unpackhi_epi16(w[0], w[0]);
  o[2] = _mm_unpacklo_epi16(w[1], w[1]);
  o[3] = _mm_unpackhi_epi16(w[1], w[1]);
  o[4] = _mm_unpacklo_epi16(w[2], w[2]);
  o[5] = _mm_unpackhi_epi16(w[2], w[2]);
  o[6] = _mm_unpacklo_epi16(w[3], w[3]);
  o[7] = _mm_unpackhi_epi16(w[3], w[3]);

  for (int i = 0; i < 4; i++)
  {
    _mm_store_si128((__m128i*)(dst + i * width) + 0, o[2 * i + 0]);
    _mm_store_si128((__m128i*)(dst + i * width) + 1, o[2 * i + 1]);
  }
}

GNU_TARGET("ssse3")
static void DecodeBytes_I4_SSSE3(u32* dst, const u8* src, const u16*, int width)
{
  __m128i r0 = _mm_load_si128((__m128i*)src);

  // Duplicate nibbles.
  __m128i i1 = r0 & _mm_set1_epi8((s8)0xF0);
  __m128i i2 = r0 & _mm_set1_epi8(0x0F);
  i1 |= _mm_srli_epi16(i1, 4);
  i2 |= _mm_slli_epi16(i2, 4);
  __m128i foo = _mm_unpacklo_epi8(i1, i2);
  __m128i bar = _mm_unpackhi_epi8(i1, i2);

  // Quadruplicate bytes.
  __m128i mask3210 = _mm_set_epi64x(0x0303030302020202, 0x0101010100000000);
  __m128i mask7654 = _mm_set_epi64x(0x0707070706060606, 0x0505050504040404);
  __m128i maskBA98 = _mm_set_epi64x(0x0B0B0B0B0A0A0A0A, 0x0909090908080808);
  __m128i maskFEDC = _mm_set_epi64x(0x0F0F0F0F0E0E0E0E, 0x0D0D0D0D0C0C0C0C);
  __m128i o[8];
  o[0] = _mm_shuffle_epi8(foo, mask3210);
  o[1] = _mm_shuffle_epi8(foo, mask7654);
  o[2] = _mm_shuffle_epi8(foo, maskBA98);
  o[3] = _mm_shuffle_epi8(foo, maskFEDC);
  o[4] = _mm_shuffle_epi8(bar, mask3210);
  o[5] = _mm_shuffle_epi8(bar, mask7654);
  o[6] = _mm_shuffle_epi8(bar, maskBA98);
  o[7] = _mm_shuffle_epi8(bar, maskFEDC);

  for (int i = 0; i < 4; i++)
  {
    _mm_store_si128((__m128i*)(dst + i * width) + 0, o[2 * i + 0]);
    _mm_store_si128((__m128i*)(dst + i * width) + 1, o[2 * i + 1]);
  }
}

GNU_TARGET("avx2")
static void DecodeBytes_I4_AVX2(u32* dst, const u8* src, const u16*, int width)
{
  __m256i r0 = _mm256_load_si256((__m256i*)src);

  // Duplicate nibbles.
  __m256i n0 = r0 & _mm256_set1_epi8((s8)0xF0);
  __m256i n1 = r0 & _mm256_set1_epi8(0x0F);
  n0 |= _mm256_srli_epi16(n0, 4);
  n1 |= _mm256_slli_epi16(n1, 4);
  __m256i lo = _mm256_unpacklo_epi8(n0, n1);
  __m256i hi = _mm256_unpackhi_epi8(n0, n1);

  // Quadruplicate bytes.
  __m256i x0 = _mm256_permute2x128_si256(lo, lo, _MM_SHUFFLE(0, 0, 0, 0));
  __m256i x1 = _mm256_permute2x128_si256(hi, hi, _MM_SHUFFLE(0, 0, 0, 0));
  __m256i x2 = _mm256_permute2x128_si256(lo, lo, _MM_SHUFFLE(0, 1, 0, 1));
  __m256i x3 = _mm256_permute2x128_si256(hi, hi, _MM_SHUFFLE(0, 1, 0, 1));
  __m256i mask0 = _mm256_set_epi64x(0x0707070706060606, 0x0505050504040404, 0x0303030302020202,
                                    0x0101010100000000);
  __m256i mask1 = _mm256_set_epi64x(0x0F0F0F0F0E0E0E0E, 0x0D0D0D0D0C0C0C0C, 0x0B0B0B0B0A0A0A0A,
                                    0x0909090908080808);
  __m256i out[8];
  out[0] = _mm256_shuffle_epi8(x0, mask0);
  out[1] = _mm256_shuffle_epi8(x0, mask1);
  out[2] = _mm256_shuffle_epi8(x1, mask0);
  out[3] = _mm256_shuffle_epi8(x1, mask1);
  out[4] = _mm256_shuffle_epi8(x2, mask0);
  out[5] = _mm256_shuffle_epi8(x2, mask1);
  out[6] = _mm256_shuffle_epi8(x3, mask0);
  out[7] = _mm256_shuffle_epi8(x3, mask1);

  for (int i = 0; i < 8; i++)
    _mm256_store_si256((__m256i*)(dst + i * width), out[i]);
}

static void DecodeBytes_I8_SSE2(u32* dst, const u8* src, const u16*, int)
{
  __m128i r0 = _mm_loadl_epi64((__m128i*)src);  // 0000 0000 hgfe dcba
  __m128i r1 = _mm_unpacklo_epi8(r0, r0);       // hhgg ffee ddcc bbaa

  __m128i rgba0 = _mm_unpacklo_epi16(r1, r1);  // dddd cccc bbbb aaaa
  __m128i rgba1 = _mm_unpackhi_epi16(r1, r1);  // hhhh gggg ffff eeee

  _mm_store_si128((__m128i*)dst + 0, rgba0);
  _mm_store_si128((__m128i*)dst + 1, rgba1);
}

GNU_TARGET("ssse3")
static void DecodeBytes_I8_SSSE3(u32* dst, const u8* src, const u16*, int)
{
  __m128i c = _mm_loadl_epi64((__m128i*)src);
  __m128i c0 = _mm_shuffle_epi8(c, _mm_set_epi64x(0x0303030302020202, 0x0101010100000000));
  __m128i c1 = _mm_shuffle_epi8(c, _mm_set_epi64x(0x0707070706060606, 0x0505050504040404));
  _mm_store_si128((__m128i*)dst + 0, c0);
  _mm_store_si128((__m128i*)dst + 1, c1);
}

GNU_TARGET("avx2")
static void DecodeBytes_I8_AVX2(u32* dst, const u8* src, const u16*, int width)
{
  __m256i c = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)src));
  __m256i shuffle0 = _mm256_set_epi64x(0x0707070706060606, 0x0505050504040404, 0x0303030302020202,
                                       0x0101010100000000);
  __m256i shuffle1 = _mm256_set_epi64x(0x0F0F0F0F0E0E0E0E, 0x0D0D0D0D0C0C0C0C, 0x0B0B0B0B0A0A0A0A,
                                       0x0909090908080808);
  __m256i row0 = _mm256_shuffle_epi8(c, shuffle0);
  __m256i row1 = _mm256_shuffle_epi8(c, shuffle1);
  _mm256_store_si256((__m256i*)(dst + 0 * width), row0);
  _mm256_store_si256((__m256i*)(dst + 1 * width), row1);
}

static inline void DecodeBytes_IA4_SSE2(u32* dst, const u8* src, const u16*, int width)
{
  __m128i c = _mm_load_si128((__m128i*)src);
  // Expand nibbles to bytes.
  __m128i nibble = _mm_set1_epi8(0x0F);
  __m128i by = c & nibble;
  __m128i te = _mm_srli_epi16(c, 4) & nibble;
  __m128i lo = _mm_unpacklo_epi8(by, te);
  __m128i hi = _mm_unpackhi_epi8(by, te);
  // Duplicate nibbles.
  lo |= _mm_slli_epi16(lo, 4);
  hi |= _mm_slli_epi16(hi, 4);
  // Convert from AI8 to RGBA.
  __m128i c0 = _mm_unpacklo_epi8(lo, lo);
  __m128i c1 = _mm_unpackhi_epi8(lo, lo);
  __m128i c2 = _mm_unpacklo_epi8(hi, hi);
  __m128i c3 = _mm_unpackhi_epi8(hi, hi);
  __m128i m = _mm_set1_epi32(0x000000FF);
  c0 = (c0 & m) | _mm_slli_epi32(c0, 8);
  c1 = (c1 & m) | _mm_slli_epi32(c1, 8);
  c2 = (c2 & m) | _mm_slli_epi32(c2, 8);
  c3 = (c3 & m) | _mm_slli_epi32(c3, 8);
  _mm_store_si128((__m128i*)(dst + 0 * width) + 0, c0);
  _mm_store_si128((__m128i*)(dst + 0 * width) + 1, c1);
  _mm_store_si128((__m128i*)(dst + 1 * width) + 0, c2);
  _mm_store_si128((__m128i*)(dst + 1 * width) + 1, c3);
}

GNU_TARGET("ssse3")
static inline void DecodeBytes_IA4_SSSE3(u32* dst, const u8* src, const u16*, int)
{
  __m128i c = _mm_loadl_epi64((__m128i*)src);
  c = _mm_unpacklo_epi8(c, _mm_setzero_si128());
  // __FE__DC__BA__98__76__54__32__10
  c |= _mm_slli_epi16(c, 4);
  // _FXE_DXC_BXA_9X8_7X6_5X4_3X2_1X0
  c &= _mm_set1_epi8(0x0F);
  // _F_E_D_C_B_A_9_8_7_6_5_4_3_2_1_0
  c |= _mm_slli_epi16(c, 4);
  // FFEEDDCCBBAA99887766554433221100
  __m128i c0 = _mm_shuffle_epi8(c, _mm_set_epi64x(0x0706060605040404, 0x0302020201000000));
  __m128i c1 = _mm_shuffle_epi8(c, _mm_set_epi64x(0x0F0E0E0E0D0C0C0C, 0x0B0A0A0A09080808));
  _mm_store_si128((__m128i*)dst + 0, c0);
  _mm_store_si128((__m128i*)dst + 1, c1);
}

GNU_TARGET("avx2")
static void DecodeBytes_IA4_AVX2(u32* dst, const u8* src, const u16*, int width)
{
  __m256i c = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)src));
  c = _mm256_shuffle_epi8(c, _mm256_set_epi64x(0xFF0FFF0EFF0DFF0C, 0xFF07FF06FF05FF04,
                                               0xFF0BFF0AFF09FF08, 0xFF03FF02FF01FF00));
  c |= _mm256_slli_epi16(c, 4);
  c &= _mm256_set1_epi8(0x0F);
  c |= _mm256_slli_epi16(c, 4);
  __m256i mask0 = _mm256_set_epi64x(0x0706060605040404, 0x0302020201000000, 0x0706060605040404,
                                    0x0302020201000000);
  __m256i mask1 = _mm256_set_epi64x(0x0F0E0E0E0D0C0C0C, 0x0B0A0A0A09080808, 0x0F0E0E0E0D0C0C0C,
                                    0x0B0A0A0A09080808);
  __m256i row0 = _mm256_shuffle_epi8(c, mask0);
  __m256i row1 = _mm256_shuffle_epi8(c, mask1);
  _mm256_store_si256((__m256i*)(dst + 0 * width), row0);
  _mm256_store_si256((__m256i*)(dst + 1 * width), row1);
}

static void DecodeBytes_IA8_SSE2(u32* dst, const u8* src, const u16*, int width)
{
  __m128i* s = (__m128i*)src;
  __m128i c = _mm_load_si128(s);          // FEDC BA98 7654 3210
  __m128i i = _mm_srli_epi16(c, 8);       // _F_D _B_9 _7_5 _3_1
  __m128i a = _mm_slli_epi16(c, 8);       // E_C_ A_8_ 6_4_ 2_0_
  a |= i;                                 // EFCD AB89 6745 2301
  i |= _mm_slli_epi64(i, 8);              // FFDD BB99 7755 3311
  __m128i c0 = _mm_unpacklo_epi16(i, a);  // 6777 4555 2333 0111
  __m128i c1 = _mm_unpackhi_epi16(i, a);  // EFFF CDDD ABBB 8999
  _mm_store_si128((__m128i*)(dst + 0 * width), c0);
  _mm_store_si128((__m128i*)(dst + 1 * width), c1);
}

GNU_TARGET("ssse3")
static void DecodeBytes_IA8_SSSE3(u32* dst, const u8* src, const u16*, int)
{
  __m128i c = _mm_loadl_epi64((__m128i*)src);
  c = _mm_shuffle_epi8(c, _mm_set_epi64x(0x0607070704050505, 0x0203030300010101));
  _mm_store_si128((__m128i*)dst, c);
}

static void DecodeBytes_RGB565_SSE2(u32* dst, const u8* src, const u16*, int)
{
  __m128i* src128 = (__m128i*)src;
  __m128i rgb565x4 = _mm_loadl_epi64(src128);
  __m128i c0 = _mm_unpacklo_epi16(rgb565x4, rgb565x4);

  // swizzle gggBBBbb RRRrrGGg gggBBBbb RRRrrGGg
  //      to 11111111 BBBbbBBB GGggggGG RRRrrRRR

  __m128i red = c0 & _mm_set1_epi32(0x000000F8);
  __m128i green = _mm_srli_epi32(c0, 3) & _mm_set1_epi32(0x0000FC00);
  __m128i blue = _mm_srli_epi32(c0, 5) & _mm_set1_epi32(0x00F80000);

  __m128i red_blue = red | blue;
  __m128i copy3 = _mm_srli_epi16(red_blue, 5);
  __m128i copy2 = _mm_srli_epi32(green, 6) & _mm_set1_epi32(0x00000300);

  __m128i alpha = _mm_set1_epi32(0xFF000000);
  __m128i rgba = (red_blue | copy3) | (green | copy2) | alpha;

  _mm_store_si128((__m128i*)dst, rgba);
}

GNU_TARGET("avx2")
static void DecodeBytes_RGB565_AVX2(u32* dst, const u8* src, const u16*, int width)
{
  __m256i c = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)src));
  c = _mm256_shuffle_epi8(c, _mm256_set_epi64x(0xFFFF0E0FFFFF0C0D, 0xFFFF0A0BFFFF0809,
                                               0xFFFF0607FFFF0405, 0xFFFF0203FFFF0001));

  // swizzle 00000000000000000 RRRrrGGg gggBBBbb
  //      to 11111111 BBBbbBBB GGggggGG RRRrrRRR

  __m256i red = _mm256_srli_epi32(c, 8);
  __m256i green = _mm256_slli_epi32(c, 5) & _mm256_set1_epi32(0x0000FC00);
  __m256i blue = _mm256_slli_epi32(c, 19);

  __m256i red_blue = (red | blue) & _mm256_set1_epi32(0x00F800F8);
  __m256i copy3 = _mm256_srli_epi16(red_blue, 5);
  __m256i copy2 = _mm256_srli_epi32(green, 6) & _mm256_set1_epi32(0x00000300);

  __m256i alpha = _mm256_set1_epi32(0xFF000000);
  __m256i rgba = (red_blue | copy3) | (green | copy2) | alpha;

  __m128i r0 = _mm256_extracti128_si256(rgba, 0);
  __m128i r1 = _mm256_extracti128_si256(rgba, 1);

  _mm_store_si128((__m128i*)(dst + 0 * width), r0);
  _mm_store_si128((__m128i*)(dst + 1 * width), r1);
}

static void DecodeBytes_RGB5A3_SSE2(u32* dst, const u8* src, const u16*, int width)
{
  __m128i val = _mm_load_si128((__m128i*)src);
  __m128i msb = _mm_slli_epi16(val, 8);
  __m128i lsb = _mm_srli_epi16(val, 8);
  val = msb | lsb;

  // RGB555
  // 1RrrrrGg gggBbbbb
  __m128i r = _mm_srli_epi16(val, 7) & _mm_set1_epi16(0x00F8);
  __m128i g = _mm_slli_epi16(val, 6) & _mm_set1_epi16((s16)0xF800);
  __m128i b = _mm_slli_epi16(val, 3) & _mm_set1_epi16(0x00F8);
  __m128i a = _mm_set1_epi16((s16)0xFF00);
  __m128i gr = g | r;
  __m128i ab = a | b;
  gr |= _mm_srli_epi16(gr, 5) & _mm_set1_epi16(0x0707);
  ab |= _mm_srli_epi16(ab, 5) & _mm_set1_epi16(0x0707);
  __m128i rgb555_0 = _mm_unpacklo_epi16(gr, ab);
  __m128i rgb555_1 = _mm_unpackhi_epi16(gr, ab);
  // 11111111 BbbbbBbb GggggGgg RrrrrRrr

  // RGBA4443
  // 00000000 00000000 0AaaRrrr GgggBbbb
  r = _mm_srli_epi16(val, 8) & _mm_set1_epi16(0x000F);
  g = _mm_slli_epi16(val, 4) & _mm_set1_epi16(0x0F00);
  b = val & _mm_set1_epi16(0x000F);
  a = _mm_slli_epi16(val, 1) & _mm_set1_epi16((s16)0xE000);
  b |= _mm_slli_epi16(b, 4);
  a |= _mm_srli_epi16(a, 3) | (_mm_srli_epi16(val, 5) & _mm_set1_epi16(0x0300));
  gr = g | r;
  ab = a | b;
  gr |= _mm_slli_epi16(gr, 4);
  __m128i rgba4443_0 = _mm_unpacklo_epi16(gr, ab);
  __m128i rgba4443_1 = _mm_unpackhi_epi16(gr, ab);

  val = _mm_srai_epi16(val, 15);
  __m128i mask0 = _mm_unpacklo_epi16(val, val);
  __m128i mask1 = _mm_unpackhi_epi16(val, val);
  __m128i final0 = (rgb555_0 & mask0) | _mm_andnot_si128(mask0, rgba4443_0);
  __m128i final1 = (rgb555_1 & mask1) | _mm_andnot_si128(mask1, rgba4443_1);
  _mm_store_si128((__m128i*)(dst + 0 * width), final0);
  _mm_store_si128((__m128i*)(dst + 1 * width), final1);
}

GNU_TARGET("ssse3")
static void DecodeBytes_RGB5A3_SSSE3(u32* dst, const u8* src, const u16*, int)
{
  // TODO: load 128 bits at once and handle them as in the SSE2 case
  __m128i val = _mm_loadl_epi64((__m128i*)src);
  val = _mm_shuffle_epi8(val, _mm_set_epi64x(0xFFFF0607FFFF0405, 0xFFFF0203FFFF0001));

  // RGB555
  // 00000000 00000000 1RrrrrGg gggBbbbb
  __m128i r = _mm_srli_epi32(val, 7);
  // 00000000 00000000 00000001 RrrrrGgg
  __m128i g = _mm_slli_epi32(val, 6) & _mm_set1_epi32(0x0000F800);
  // 00000000 001Rrrrr GggggBbb bb000000
  __m128i b = _mm_slli_epi32(val, 19);
  // rrrGgggg Bbbbb000 00000000 00000000
  __m128i rgb555 = (r | b) & _mm_set1_epi32(0x00F800F8);
  // 00000000 Bbbbb000 00000000 Rrrrr000
  rgb555 |= g;
  // 00000000 Bbbbb000 Ggggg000 Rrrrr000
  rgb555 |= _mm_srli_epi32(rgb555, 5) & _mm_set1_epi32(0x00070707);
  // 00000000 BbbbbBbb GggggGgg RrrrrRrr
  rgb555 |= _mm_set1_epi32(0xFF000000);
  // 11111111 BbbbbBbb GggggGgg RrrrrRrr

  // RGBA4443
  // 00000000 00000000 0AaaRrrr GgggBbbb
  __m128i mrb = _mm_shuffle_epi8(val, _mm_set_epi64x(0x0D0CFF0D0908FF09, 0x0504FF050100FF01));
  // 0AaaRrrr GgggBbbb 00000000 0AaaRrrr
  g = _mm_slli_epi16(val, 4);
  // 00000000 00000000 RrrrGggg Bbbb0000
  __m128i rgba4443 = (mrb | g) & _mm_set1_epi32(0x000F0F0F);
  // 00000000 0000Bbbb 0000Gggg 0000Rrrr
  rgba4443 |= _mm_slli_epi32(rgba4443, 4);
  // 00000000 BbbbBbbb GgggGggg RrrrRrrr

  __m128i a = _mm_slli_epi32(val, 17) & _mm_set1_epi32(0xE0000000);
  a |= _mm_srli_epi32(a, 3) | _mm_slli_epi32(val & _mm_set1_epi32(0x00006000), 11);
  rgba4443 |= a;
  // AaaAaaAa BbbbBbbb GgggGggg RrrrRrrr

  __m128i mask = _mm_srai_epi32(mrb, 31);
  rgb555 &= mask;
  rgba4443 = _mm_andnot_si128(mask, rgba4443);

  _mm_store_si128((__m128i*)dst, rgb555 | rgba4443);
}

// Input: duplicated 16 bit big endian RGB5A3
GNU_TARGET("sse4.1")
static __m128i DecodePixels_RGB5A3_SSE41(__m128i input)
{
  __m128i red, green, blue, alpha, blue_red, blue_green_red;

  // GggBBBbb 1RRRrrGG GggBBBbb 1RRRrrGG
  red = _mm_slli_epi32(input, 1) & _mm_set1_epi32(0x000000F8);
  // 00000000 00000000 00000000 RRRrr000
  blue = _mm_slli_epi32(input, 11) & _mm_set1_epi32(0x00F80000);
  // 00000000 BBBbb000 00000000 00000000
  blue_red = blue | red;
  blue_red |= _mm_srli_epi16(blue_red, 5);
  // 00000000 BBBbbBBB 00000000 RRRrrRRR
  green = _mm_srli_epi32(input, 2) & _mm_set1_epi32(0x0000F800);
  green |= _mm_slli_epi32(_mm_srli_epi32(green, 13), 8);
  // 00000000 00000000 GGGggGGG 00000000
  alpha = _mm_set1_epi32(0xFF000000);
  __m128i rgb555 = alpha | blue_red | green;
  // 11111111 BBBbbBBB GGGggGGG RRRrrRRR

  // GGggBBbb 0AaaRRrr GGggBBbb 0AaaRRrr
  red = input & _mm_set1_epi32(0x0000000F);
  green = _mm_srli_epi32(input & _mm_set1_epi32(0x0000F000), 4);
  blue = _mm_slli_epi32(input & _mm_set1_epi32(0x00000F00), 8);
  alpha = _mm_slli_epi32(_mm_srli_epi32(input, 4), 29);
  blue_green_red = blue | green | red;
  blue_green_red |= _mm_slli_epi32(blue_green_red, 4);
  alpha |= _mm_srli_epi32(alpha, 3);
  alpha |= _mm_slli_epi32(input & _mm_set1_epi32(0x00000060), 19);
  __m128i argb3444 = alpha | blue_green_red;
  // AaaAaaAa BBbbBBbb GGggGGgg RRrrRRrr

  __m128i blend_mask =
      _mm_shuffle_epi8(input, _mm_set_epi64x(0x0C0C0C0C08080808, 0x0404040400000000));
  return _mm_blendv_epi8(argb3444, rgb555, blend_mask);
}

GNU_TARGET("sse4.1")
static void DecodeBytes_RGB5A3_SSE41(u32* dst, const u8* src, const u16*, int)
{
  __m128i pixels = _mm_loadl_epi64((__m128i*)src);
  // Duplicate big endian 16 bit colors to 32 bits.
  pixels = _mm_unpacklo_epi16(pixels, pixels);
  _mm_store_si128((__m128i*)dst, DecodePixels_RGB5A3_SSE41(pixels));
}

GNU_TARGET("avx2")
static __m256i DecodePixels_RGB5A3_AVX2(__m256i input)
{
  __m256i red, green, blue, alpha, blue_red, blue_green_red;

  // GggBBBbb 1RRRrrGG GggBBBbb 1RRRrrGG
  red = _mm256_slli_epi32(input, 1) & _mm256_set1_epi32(0x000000F8);
  // 00000000 00000000 00000000 RRRrr000
  blue = _mm256_slli_epi32(input, 11) & _mm256_set1_epi32(0x00F80000);
  // 00000000 BBBbb000 00000000 00000000
  blue_red = blue | red;
  blue_red |= _mm256_srli_epi16(blue_red, 5);
  // 00000000 BBBbbBBB 00000000 RRRrrRRR
  green = _mm256_srli_epi32(input, 2) & _mm256_set1_epi32(0x0000F800);
  green |= _mm256_slli_epi32(_mm256_srli_epi32(green, 13), 8);
  // 00000000 00000000 GGGggGGG 00000000
  alpha = _mm256_set1_epi32(0xFF000000);
  __m256i rgb555 = alpha | blue_red | green;
  // 11111111 BBBbbBBB GGGggGGG RRRrrRRR

  // GGggBBbb 0AaaRRrr GGggBBbb 0AaaRRrr
  red = input & _mm256_set1_epi32(0x0000000F);
  green = _mm256_srli_epi32(input & _mm256_set1_epi32(0x0000F000), 4);
  blue = _mm256_slli_epi32(input & _mm256_set1_epi32(0x00000F00), 8);
  alpha = _mm256_slli_epi32(_mm256_srli_epi32(input, 4), 29);
  blue_green_red = blue | green | red;
  blue_green_red |= _mm256_slli_epi32(blue_green_red, 4);
  alpha |= _mm256_srli_epi32(alpha, 3);
  alpha |= _mm256_slli_epi32(input & _mm256_set1_epi32(0x00000060), 19);
  __m256i argb3444 = alpha | blue_green_red;
  // AaaAaaAa BBbbBBbb GGggGGgg RRrrRRrr

  __m256i blend_mask =
      _mm256_shuffle_epi8(input, _mm256_set_epi64x(0x0C0C0C0C08080808, 0x0404040400000000,
                                                   0x0C0C0C0C08080808, 0x0404040400000000));
  return _mm256_blendv_epi8(argb3444, rgb555, blend_mask);
}

GNU_TARGET("avx2")
static void DecodeBytes_RGB5A3_AVX2(u32* dst, const u8* src, const u16*, int width)
{
  __m256i pixels = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)src));
  pixels = _mm256_shuffle_epi8(pixels, _mm256_set_epi64x(0x0F0E0F0E0D0C0D0C, 0x0B0A0B0A09080908,
                                                         0x0706070605040504, 0x0302030201000100));
  pixels = DecodePixels_RGB5A3_AVX2(pixels);
  __m128i row0 = _mm256_extracti128_si256(pixels, 0);
  __m128i row1 = _mm256_extracti128_si256(pixels, 1);
  _mm_store_si128((__m128i*)(dst + 0 * width), row0);
  _mm_store_si128((__m128i*)(dst + 1 * width), row1);
}

static void DecodeBytes_RGBA8_SSE2(u32* dst, const u8* src, const u16*, int width)
{
  // Input is divided up into 16-bit words. The texels are split up into AR and
  // GB components where all
  // AR components come grouped up first in 32 bytes followed by the GB
  // components in 32 bytes. We are
  // processing 16 texels per each loop iteration, numbered from 0-f.
  //
  // Convention is:
  //   one byte is [component-name texel-number]
  //    __m128i is (4-bytes 4-bytes 4-bytes 4-bytes)
  //
  // Input  is ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2]
  // [A 1][R 1][A 0][R 0])
  //           ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a]
  //           [A 9][R 9][A 8][R 8])
  //           ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2]
  //           [G 1][B 1][G 0][B 0])
  //           ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a]
  //           [G 9][B 9][G 8][B 8])
  //
  // Output is (RGBA3 RGBA2 RGBA1 RGBA0)
  //           (RGBA7 RGBA6 RGBA5 RGBA4)
  //           (RGBAb RGBAa RGBA9 RGBA8)
  //           (RGBAf RGBAe RGBAd RGBAc)

  // Loads the 1st half of AR components ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R
  // 4] [A 3][R 3][A 2][R 2] [A 1][R 1][A 0][R 0])
  const __m128i ar0 = _mm_load_si128((__m128i*)src + 0);
  // Loads the 2nd half of AR components ([A f][R f][A e][R e] [A d][R d][A c][R
  // c] [A b][R b][A a][R a] [A 9][R 9][A 8][R 8])
  const __m128i ar1 = _mm_load_si128((__m128i*)src + 1);
  // Loads the 1st half of GB components ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B
  // 4] [G 3][B 3][G 2][B 2] [G 1][B 1][G 0][B 0])
  const __m128i gb0 = _mm_load_si128((__m128i*)src + 2);
  // Loads the 2nd half of GB components ([G f][B f][G e][B e] [G d][B d][G c][B
  // c] [G b][B b][G a][B a] [G 9][B 9][G 8][B 8])
  const __m128i gb1 = _mm_load_si128((__m128i*)src + 3);
  // Expand the AR components to fill out 32-bit words:
  // ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R
  // 1][A 0][R 0]) -> ([A 3][A 3][R 3][R 3] [A 2][A 2][R 2][R 2] [A 1][A 1][R
  // 1][R 1] [A 0][A 0][R 0][R 0])
  const __m128i aarr00 = _mm_unpacklo_epi8(ar0, ar0);
  // ([A 7][R 7][A 6][R 6] [A 5][R 5][A 4][R 4] [A 3][R 3][A 2][R 2] [A 1][R
  // 1][A 0][R 0]) -> ([A 7][A 7][R 7][R 7] [A 6][A 6][R 6][R 6] [A 5][A 5][R
  // 5][R 5] [A 4][A 4][R 4][R 4])
  const __m128i aarr01 = _mm_unpackhi_epi8(ar0, ar0);
  // ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R
  // 9][A 8][R 8]) -> ([A b][A b][R b][R b] [A a][A a][R a][R a] [A 9][A 9][R
  // 9][R 9] [A 8][A 8][R 8][R 8])
  const __m128i aarr10 = _mm_unpacklo_epi8(ar1, ar1);
  // ([A f][R f][A e][R e] [A d][R d][A c][R c] [A b][R b][A a][R a] [A 9][R
  // 9][A 8][R 8]) -> ([A f][A f][R f][R f] [A e][A e][R e][R e] [A d][A d][R
  // d][R d] [A c][A c][R c][R c])
  const __m128i aarr11 = _mm_unpackhi_epi8(ar1, ar1);

  const __m128i kMask_x0ff0 = _mm_set1_epi32(0x00FFFF00);
  // Move A right 24 bits to get A in its final place:
  const __m128i ___a00 = _mm_srli_epi32(aarr00, 24);
  // Move R left  24 bits to get R in its final place:
  const __m128i r___00 = _mm_slli_epi32(aarr00, 24);
  // OR the two together to get R and A in their final places:
  const __m128i r__a00 = r___00 | ___a00;

  const __m128i ___a01 = _mm_srli_epi32(aarr01, 24);
  const __m128i r___01 = _mm_slli_epi32(aarr01, 24);
  const __m128i r__a01 = r___01 | ___a01;

  const __m128i ___a10 = _mm_srli_epi32(aarr10, 24);
  const __m128i r___10 = _mm_slli_epi32(aarr10, 24);
  const __m128i r__a10 = r___10 | ___a10;

  const __m128i ___a11 = _mm_srli_epi32(aarr11, 24);
  const __m128i r___11 = _mm_slli_epi32(aarr11, 24);
  const __m128i r__a11 = r___11 | ___a11;

  // Expand the GB components to fill out 32-bit words:
  // ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B
  // 1][G 0][B 0]) -> ([G 3][G 3][B 3][B 3] [G 2][G 2][B 2][B 2] [G 1][G 1][B
  // 1][B 1] [G 0][G 0][B 0][B 0])
  const __m128i ggbb00 = _mm_unpacklo_epi8(gb0, gb0);
  // ([G 7][B 7][G 6][B 6] [G 5][B 5][G 4][B 4] [G 3][B 3][G 2][B 2] [G 1][B
  // 1][G 0][B 0]) -> ([G 7][G 7][B 7][B 7] [G 6][G 6][B 6][B 6] [G 5][G 5][B
  // 5][B 5] [G 4][G 4][B 4][B 4])
  const __m128i ggbb01 = _mm_unpackhi_epi8(gb0, gb0);
  // ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B
  // 9][G 8][B 8]) -> ([G b][G b][B b][B b] [G a][G a][B a][B a] [G 9][G 9][B
  // 9][B 9] [G 8][G 8][B 8][B 8])
  const __m128i ggbb10 = _mm_unpacklo_epi8(gb1, gb1);
  // ([G f][B f][G e][B e] [G d][B d][G c][B c] [G b][B b][G a][B a] [G 9][B
  // 9][G 8][B 8]) -> ([G f][G f][B f][B f] [G e][G e][B e][B e] [G d][G d][B
  // d][B d] [G c][G c][B c][B c])
  const __m128i ggbb11 = _mm_unpackhi_epi8(gb1, gb1);

  // G and B are already in perfect spots in the center, just remove the extra
  // copies in the 1st and 4th positions:
  const __m128i _gb_00 = ggbb00 & kMask_x0ff0;
  const __m128i _gb_01 = ggbb01 & kMask_x0ff0;
  const __m128i _gb_10 = ggbb10 & kMask_x0ff0;
  const __m128i _gb_11 = ggbb11 & kMask_x0ff0;

  // Now join up R__A and _GB_ to get RGBA!
  __m128i rgba00 = r__a00 | _gb_00;
  __m128i rgba01 = r__a01 | _gb_01;
  __m128i rgba10 = r__a10 | _gb_10;
  __m128i rgba11 = r__a11 | _gb_11;
  // Write em out!
  _mm_store_si128((__m128i*)(dst + 0 * width), rgba00);
  _mm_store_si128((__m128i*)(dst + 1 * width), rgba01);
  _mm_store_si128((__m128i*)(dst + 2 * width), rgba10);
  _mm_store_si128((__m128i*)(dst + 3 * width), rgba11);
}

GNU_TARGET("ssse3")
static void DecodeBytes_RGBA8_SSSE3(u32* dst, const u8* src, const u16*, int width)
{
  __m128i mask0312 = _mm_set_epi32(0x0C0F0D0E, 0x080B090A, 0x04070506, 0x00030102);
  __m128i ar0 = _mm_load_si128((__m128i*)src + 0);
  __m128i ar1 = _mm_load_si128((__m128i*)src + 1);
  __m128i gb0 = _mm_load_si128((__m128i*)src + 2);
  __m128i gb1 = _mm_load_si128((__m128i*)src + 3);

  __m128i rgba[4];
  rgba[0] = _mm_shuffle_epi8(_mm_unpacklo_epi8(ar0, gb0), mask0312);
  rgba[1] = _mm_shuffle_epi8(_mm_unpackhi_epi8(ar0, gb0), mask0312);
  rgba[2] = _mm_shuffle_epi8(_mm_unpacklo_epi8(ar1, gb1), mask0312);
  rgba[3] = _mm_shuffle_epi8(_mm_unpackhi_epi8(ar1, gb1), mask0312);

  for (int i = 0; i < 4; i++)
    _mm_store_si128((__m128i*)(dst + i * width), rgba[i]);
}

GNU_TARGET("avx2")
static void DecodeBytes_RGBA8_AVX2(u32* dst, const u8* src, const u16*, int width)
{
  __m256i ar = _mm256_load_si256((__m256i*)src + 0);
  __m256i gb = _mm256_load_si256((__m256i*)src + 1);

  __m256i mask0312 = _mm256_set_epi32(0x0C0F0D0E, 0x080B090A, 0x04070506, 0x00030102, 0x0C0F0D0E,
                                      0x080B090A, 0x04070506, 0x00030102);
  __m256i rgba0 = _mm256_shuffle_epi8(_mm256_unpacklo_epi8(ar, gb), mask0312);
  __m256i rgba1 = _mm256_shuffle_epi8(_mm256_unpackhi_epi8(ar, gb), mask0312);

  __m128i rgba00 = _mm256_extracti128_si256(rgba0, 0);
  __m128i rgba01 = _mm256_extracti128_si256(rgba1, 0);
  __m128i rgba10 = _mm256_extracti128_si256(rgba0, 1);
  __m128i rgba11 = _mm256_extracti128_si256(rgba1, 1);

  _mm_store_si128((__m128i*)(dst + 0 * width), rgba00);
  _mm_store_si128((__m128i*)(dst + 1 * width), rgba01);
  _mm_store_si128((__m128i*)(dst + 2 * width), rgba10);
  _mm_store_si128((__m128i*)(dst + 3 * width), rgba11);
}

template <TlutFormat format, bool swap = false>
GNU_TARGET("avx2")
static __m256i DecodePixels_AVX2(__m256i pixels)
{
  switch (format)
  {
  case GX_TL_IA8:
  {
    __m256i shuffle;
    if (swap)
      shuffle = _mm256_set_epi64x(0x080909090C0D0D0D, 0x0001010104050505, 0x080909090C0D0D0D,
                                  0x0001010104050505);
    else
      shuffle = _mm256_set_epi64x(0x0C0D0D0D08090909, 0x0405050500010101, 0x0C0D0D0D08090909,
                                  0x0405050500010101);
    return _mm256_shuffle_epi8(pixels, shuffle);
  }
  case GX_TL_RGB565:
  {
    __m256i shuffle;
    if (swap)
      shuffle = _mm256_set_epi64x(0xFFFF0809FFFF0C0D, 0xFFFF0001FFFF0405, 0xFFFF0809FFFF0C0D,
                                  0xFFFF0001FFFF0405);
    else
      shuffle = _mm256_set_epi64x(0xFFFF0C0DFFFF0809, 0xFFFF0405FFFF0001, 0xFFFF0C0DFFFF0809,
                                  0xFFFF0405FFFF0001);
    __m256i c = _mm256_shuffle_epi8(pixels, shuffle);
    __m256i red = _mm256_srli_epi32(c, 8);
    __m256i green = _mm256_slli_epi32(c, 5) & _mm256_set1_epi32(0x0000FC00);
    __m256i blue = _mm256_slli_epi32(c, 19);

    __m256i red_blue = (red | blue) & _mm256_set1_epi32(0x00F800F8);
    __m256i copy3 = _mm256_srli_epi16(red_blue, 5);
    __m256i copy2 = _mm256_srli_epi32(green, 6) & _mm256_set1_epi32(0x00000300);

    __m256i alpha = _mm256_set1_epi32(0xFF000000);
    return (red_blue | copy3) | (green | copy2) | alpha;
  }
  case GX_TL_RGB5A3:
    if (swap)
    {
      pixels =
          _mm256_shuffle_epi8(pixels, _mm256_set_epi64x(0x090809080D0C0D0C, 0x0100010005040504,
                                                        0x090809080D0C0D0C, 0x0100010005040504));
    }
    else
    {
      pixels &= _mm256_set1_epi32(0x0000FFFF);
      pixels |= _mm256_slli_epi32(pixels, 16);
    }
    return DecodePixels_RGB5A3_AVX2(pixels);
  default:
    return _mm256_setzero_si256();
  }
}

template <TlutFormat tlut_format>
GNU_TARGET("avx2")
static void DecodeBytes_C4_AVX2(u32* dst, const u8* src, const u16* tlut, int width)
{
  // Loop invariant and recognized as such, at least in Clang.
  __m256i palette0 = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)tlut + 0));
  __m256i palette1 = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)tlut + 1));

  // Load 32 index nibbles, duplicate them, and move them into dword positions.
  __m256i indices_orig = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)src));
  __m256i shuffle = _mm256_set_epi64x(0xFFFFFF0303FF0303, 0xFFFFFF0202FF0202, 0xFFFFFF0101FF0101,
                                      0xFFFFFF0000FF0000);
  for (int i = 0; i < 4; i++)
  {
    __m256i indices = _mm256_shuffle_epi8(indices_orig, shuffle);
    shuffle = _mm256_add_epi8(shuffle, _mm256_set1_epi8(4));
    indices &= _mm256_set1_epi64x(0x000000F0F0000F0F);
    indices |= _mm256_slli_epi64(indices, 4);
    indices &= _mm256_set1_epi32(0x00000F0F);

    // Can't byte-shuffle across lanes, so need to use two shuffles + blend with
    // uppermost index bit as mask.
    __m256i mask = indices;
    indices &= _mm256_set1_epi32(0x00000707);
    // Convert color indices into byte indices.
    indices = _mm256_slli_epi64(indices, 1);
    indices |= _mm256_set1_epi32(0x00000100);
    // Do the actual palette lookups.
    __m256i color0 = _mm256_shuffle_epi8(palette0, indices);
    __m256i color1 = _mm256_shuffle_epi8(palette1, indices);
    // Sign-extend the uppermost bit of each index to a full dword mask.
    mask = _mm256_slli_epi32(mask, 28);
    mask = _mm256_srai_epi32(mask, 31);
    __m256i color = _mm256_blendv_epi8(color0, color1, mask);
    // Swap (upper nibble is actually the first index) and convert to ABGR8.
    color = DecodePixels_AVX2<tlut_format, true>(color);
    _mm256_store_si256((__m256i*)(dst + i * width), color);
  }
}

template <TlutFormat tlut_format>
GNU_TARGET("avx2")
static void DecodeBytes_C8_AVX2(u32* dst, const u8* src, const u16* tlut, int width)
{
  __m256i indices = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)src));
  // Expand indices.
  __m256i i0 =
      _mm256_shuffle_epi8(indices, _mm256_set_epi64x(0xFFFFFF07FFFFFF06, 0xFFFFFF05FFFFFF04,
                                                     0xFFFFFF03FFFFFF02, 0xFFFFFF01FFFFFF00));
  __m256i i1 =
      _mm256_shuffle_epi8(indices, _mm256_set_epi64x(0xFFFFFF0FFFFFFF0E, 0xFFFFFF0DFFFFFF0C,
                                                     0xFFFFFF0BFFFFFF0A, 0xFFFFFF09FFFFFF08));
  // FIXME: this can overread by two bytes!
  __m256i c0 = _mm256_i32gather_epi32((int*)tlut, i0, 2);
  __m256i c1 = _mm256_i32gather_epi32((int*)tlut, i1, 2);
  c0 = DecodePixels_AVX2<tlut_format>(c0);
  c1 = DecodePixels_AVX2<tlut_format>(c1);
  _mm256_store_si256((__m256i*)(dst + 0 * width), c0);
  _mm256_store_si256((__m256i*)(dst + 1 * width), c1);
}

template <TlutFormat tlut_format>
GNU_TARGET("avx2")
static void DecodeBytes_C14_AVX2(u32* dst, const u8* src, const u16* tlut, int width)
{
  __m256i indices = _mm256_broadcastsi128_si256(_mm_load_si128((__m128i*)src));
  __m256i expand = _mm256_set_epi64x(0xFFFF0E0FFFFF0C0D, 0xFFFF0A0BFFFF0809, 0xFFFF0607FFFF0405,
                                     0xFFFF0203FFFF0001);
  indices = _mm256_shuffle_epi8(indices, expand);
  indices &= _mm256_set1_epi32(0x00003FFF);
  // FIXME: this can overread by two bytes!
  __m256i c = _mm256_i32gather_epi32((int*)tlut, indices, 2);
  c = DecodePixels_AVX2<tlut_format>(c);
  __m128i row0 = _mm256_extracti128_si256(c, 0);
  __m128i row1 = _mm256_extracti128_si256(c, 1);
  _mm_store_si128((__m128i*)(dst + 0 * width), row0);
  _mm_store_si128((__m128i*)(dst + 1 * width), row1);
}

struct DXTBlock
{
  u32 colors;
  u32 indices;
};

static void DecodeDXT_SSE2(u32* dst, const u8* src, const u16*, int width, int height)
{
  for (int y = 0, ofs = 0; y < height; y += 8)
  {
    for (int x = 0; x < width; x += 8)
    {
      // We handle two DXT blocks simultaneously to take full advantage of
      // SSE2's 128-bit registers. This is ideal because a single DXT block
      // contains 2 RGBA colors when decoded from their 16-bit. Two DXT blocks
      // therefore contain 4 RGBA colors to be processed.
      for (int iy = 0; iy < 8; iy += 4, ofs += 2 * sizeof(DXTBlock))
      {
        // Load 128 bits, i.e. two DXTBlocks (64-bits each)
        const __m128i dxt = _mm_load_si128((__m128i*)(src + ofs));

        // Copy the 2-bit indices from each DXT block:
        alignas(16) u32 dxttmp[4];
        _mm_store_si128((__m128i*)dxttmp, dxt);

        u32 dxt0sel = dxttmp[1];
        u32 dxt1sel = dxttmp[3];

        __m128i argb888x4;
        __m128i c0 = _mm_unpacklo_epi16(dxt, dxt);
        __m128i c1 = _mm_unpackhi_epi16(dxt, dxt);
        c0 = _mm_srli_si128(_mm_slli_si128(c0, 8), 8);
        c1 = _mm_slli_si128(c1, 8);
        c0 |= c1;

        // Compare rgb0 to rgb1:
        // Each 32-bit word will contain either 0xFFFFFFFF or 0x00000000 for
        // true/false.
        const __m128i c0cmp = _mm_srli_epi32(_mm_slli_epi32(_mm_srli_epi64(c0, 8), 16), 16);
        const __m128i c0shr = _mm_srli_epi64(c0cmp, 32);
        const __m128i cmprgb0rgb1 = _mm_cmpgt_epi32(c0cmp, c0shr);

        int cmp0 = _mm_extract_epi16(cmprgb0rgb1, 0);
        int cmp1 = _mm_extract_epi16(cmprgb0rgb1, 4);

        // green:
        const __m128i gtmp = _mm_srli_epi32(c0, 3);
        const __m128i g0 = gtmp & _mm_set1_epi32(0x0000FC00);
        const __m128i g1 = _mm_srli_epi32(gtmp, 6) & _mm_set1_epi32(0x00000300);
        argb888x4 = g0 | g1;
        // red:
        const __m128i r0 = c0 & _mm_set1_epi32(0x000000F8);
        const __m128i r1 = _mm_srli_epi32(r0, 5);
        argb888x4 |= r0 | r1;
        // blue:
        const __m128i b0 = _mm_srli_epi32(c0, 5) & _mm_set1_epi32(0x00F80000);
        const __m128i b1 = _mm_srli_epi16(b0, 5);
        // OR in the fixed alpha component
        argb888x4 |= _mm_set1_epi32(0xFF000000) | b0 | b1;
        // calculate RGB2 and RGB3:
        const __m128i rgb0 = _mm_shuffle_epi32(argb888x4, _MM_SHUFFLE(2, 2, 0, 0));
        const __m128i rgb1 = _mm_shuffle_epi32(argb888x4, _MM_SHUFFLE(3, 3, 1, 1));
        const __m128i mask0f = _mm_set1_epi16(0x00FF);
        const __m128i rrggbb0 = _mm_unpacklo_epi8(rgb0, rgb0) & mask0f;
        const __m128i rrggbb1 = _mm_unpacklo_epi8(rgb1, rgb1) & mask0f;
        const __m128i rrggbb01 = _mm_unpackhi_epi8(rgb0, rgb0) & mask0f;
        const __m128i rrggbb11 = _mm_unpackhi_epi8(rgb1, rgb1) & mask0f;

        __m128i rgb2, rgb3;

        // if (rgb0 > rgb1):
        if (cmp0 != 0)
        {
          // RGB2a = ((RGB1 - RGB0) >> 1) - ((RGB1 - RGB0) >> 3)  using
          // arithmetic shifts to extend sign (not logical shifts)
          const __m128i rrggbbsub = _mm_subs_epi16(rrggbb1, rrggbb0);
          const __m128i rrggbbsubshr1 = _mm_srai_epi16(rrggbbsub, 1);
          const __m128i rrggbbsubshr3 = _mm_srai_epi16(rrggbbsub, 3);
          const __m128i shr1subshr3 = _mm_sub_epi16(rrggbbsubshr1, rrggbbsubshr3);
          const __m128i rrggbbdelta = shr1subshr3 & mask0f;
          const __m128i rgbdeltadup = _mm_packus_epi16(rrggbbdelta, rrggbbdelta);
          const __m128i rgbdelta = _mm_srli_si128(_mm_slli_si128(rgbdeltadup, 8), 8);

          rgb2 = _mm_add_epi8(rgb0, rgbdelta) & _mm_set_epi64x(0, ~0);
          rgb3 = _mm_sub_epi8(rgb1, rgbdelta) & _mm_set_epi64x(0, ~0);
        }
        else
        {
          // RGB2b = avg(RGB0, RGB1)
          const __m128i rrggbb21 = _mm_avg_epu16(rrggbb0, rrggbb1);
          const __m128i rgb210 = _mm_srli_si128(_mm_packus_epi16(rrggbb21, rrggbb21), 8);
          rgb2 = rgb210;
          rgb3 = rgb210 & _mm_set1_epi32(0x00FFFFFF);
        }

        // if (rgb0 > rgb1):
        if (cmp1 != 0)
        {
          // RGB2a = ((RGB1 - RGB0) >> 1) - ((RGB1 - RGB0) >> 3)  using
          // arithmetic shifts to extend sign (not logical shifts)
          const __m128i rrggbbsub1 = _mm_subs_epi16(rrggbb11, rrggbb01);
          const __m128i rrggbbsubshr11 = _mm_srai_epi16(rrggbbsub1, 1);
          const __m128i rrggbbsubshr31 = _mm_srai_epi16(rrggbbsub1, 3);
          const __m128i shr1subshr31 = _mm_sub_epi16(rrggbbsubshr11, rrggbbsubshr31);
          const __m128i rrggbbdelta1 = shr1subshr31 & mask0f;
          __m128i rgbdelta1 = _mm_packus_epi16(rrggbbdelta1, rrggbbdelta1);
          rgbdelta1 = _mm_slli_si128(rgbdelta1, 8);

          rgb2 |= _mm_add_epi8(rgb0, rgbdelta1) & _mm_set_epi64x(~0, 0);
          rgb3 |= _mm_sub_epi8(rgb1, rgbdelta1) & _mm_set_epi64x(~0, 0);
        }
        else
        {
          // RGB2b = avg(RGB0, RGB1)
          const __m128i rrggbb211 = _mm_avg_epu16(rrggbb01, rrggbb11);
          const __m128i rgb211 = _mm_slli_si128(_mm_packus_epi16(rrggbb211, rrggbb211), 8);
          rgb2 |= rgb211;

          // Make this color fully transparent:
          rgb3 |= rgb2 & _mm_set1_epi32(0x00FFFFFF) & _mm_set_epi64x(~0, 0);
        }

        // Create an array for color lookups for DXT0 so we can use the 2-bit
        // indices:
        const __m128i mmcolors0 =
            _mm_srli_si128(_mm_slli_si128(argb888x4, 8), 8) |
            _mm_slli_si128(_mm_srli_si128(_mm_slli_si128(rgb2, 8), 8 + 4), 8) |
            _mm_slli_si128(_mm_srli_si128(rgb3, 4), 8 + 4);

        // Create an array for color lookups for DXT1 so we can use the 2-bit
        // indices:
        const __m128i mmcolors1 = _mm_srli_si128(argb888x4, 8) |
                                  _mm_slli_si128(_mm_srli_si128(rgb2, 8 + 4), 8) |
                                  _mm_slli_si128(_mm_srli_si128(rgb3, 8 + 4), 8 + 4);

        u32* dst32 = dst + (y + iy) * width + x;

        // Copy the colors here:
        alignas(16) u32 colors0[4];
        alignas(16) u32 colors1[4];
        _mm_store_si128((__m128i*)colors0, mmcolors0);
        _mm_store_si128((__m128i*)colors1, mmcolors1);

        // Row 0:
        dst32[(width * 0) + 0] = colors0[(dxt0sel >> ((0 * 8) + 6)) & 3];
        dst32[(width * 0) + 1] = colors0[(dxt0sel >> ((0 * 8) + 4)) & 3];
        dst32[(width * 0) + 2] = colors0[(dxt0sel >> ((0 * 8) + 2)) & 3];
        dst32[(width * 0) + 3] = colors0[(dxt0sel >> ((0 * 8) + 0)) & 3];
        dst32[(width * 0) + 4] = colors1[(dxt1sel >> ((0 * 8) + 6)) & 3];
        dst32[(width * 0) + 5] = colors1[(dxt1sel >> ((0 * 8) + 4)) & 3];
        dst32[(width * 0) + 6] = colors1[(dxt1sel >> ((0 * 8) + 2)) & 3];
        dst32[(width * 0) + 7] = colors1[(dxt1sel >> ((0 * 8) + 0)) & 3];
        // Row 1:
        dst32[(width * 1) + 0] = colors0[(dxt0sel >> ((1 * 8) + 6)) & 3];
        dst32[(width * 1) + 1] = colors0[(dxt0sel >> ((1 * 8) + 4)) & 3];
        dst32[(width * 1) + 2] = colors0[(dxt0sel >> ((1 * 8) + 2)) & 3];
        dst32[(width * 1) + 3] = colors0[(dxt0sel >> ((1 * 8) + 0)) & 3];
        dst32[(width * 1) + 4] = colors1[(dxt1sel >> ((1 * 8) + 6)) & 3];
        dst32[(width * 1) + 5] = colors1[(dxt1sel >> ((1 * 8) + 4)) & 3];
        dst32[(width * 1) + 6] = colors1[(dxt1sel >> ((1 * 8) + 2)) & 3];
        dst32[(width * 1) + 7] = colors1[(dxt1sel >> ((1 * 8) + 0)) & 3];
        // Row 2:
        dst32[(width * 2) + 0] = colors0[(dxt0sel >> ((2 * 8) + 6)) & 3];
        dst32[(width * 2) + 1] = colors0[(dxt0sel >> ((2 * 8) + 4)) & 3];
        dst32[(width * 2) + 2] = colors0[(dxt0sel >> ((2 * 8) + 2)) & 3];
        dst32[(width * 2) + 3] = colors0[(dxt0sel >> ((2 * 8) + 0)) & 3];
        dst32[(width * 2) + 4] = colors1[(dxt1sel >> ((2 * 8) + 6)) & 3];
        dst32[(width * 2) + 5] = colors1[(dxt1sel >> ((2 * 8) + 4)) & 3];
        dst32[(width * 2) + 6] = colors1[(dxt1sel >> ((2 * 8) + 2)) & 3];
        dst32[(width * 2) + 7] = colors1[(dxt1sel >> ((2 * 8) + 0)) & 3];
        // Row 3:
        dst32[(width * 3) + 0] = colors0[(dxt0sel >> ((3 * 8) + 6)) & 3];
        dst32[(width * 3) + 1] = colors0[(dxt0sel >> ((3 * 8) + 4)) & 3];
        dst32[(width * 3) + 2] = colors0[(dxt0sel >> ((3 * 8) + 2)) & 3];
        dst32[(width * 3) + 3] = colors0[(dxt0sel >> ((3 * 8) + 0)) & 3];
        dst32[(width * 3) + 4] = colors1[(dxt1sel >> ((3 * 8) + 6)) & 3];
        dst32[(width * 3) + 5] = colors1[(dxt1sel >> ((3 * 8) + 4)) & 3];
        dst32[(width * 3) + 6] = colors1[(dxt1sel >> ((3 * 8) + 2)) & 3];
        dst32[(width * 3) + 7] = colors1[(dxt1sel >> ((3 * 8) + 0)) & 3];
      }
    }
  }
}

GNU_TARGET("avx2,bmi2")
static void DecodeDXT_AVX2(u32* dst, const u8* src, const u16*, int width, int height)
{
  for (int y = 0, ofs = 0; y < height; y += 8)
  {
    for (int x = 0; x < width; x += 8)
    {
      for (int iy = 0; iy < 8; iy += 4, ofs += 2 * sizeof(DXTBlock))
      {
        // IACA_START
        u32* dst32 = dst + (y + iy) * width + x;
        DXTBlock* pdxt = (DXTBlock*)(src + ofs);

        // Load two DXT blocks.
        __m128i colors565 = _mm_load_si128((__m128i*)pdxt);

        // RGB565 -> ABGR8888
        colors565 =
            _mm_shuffle_epi8(colors565, _mm_set_epi64x(0xFFFF0A0BFFFF0809, 0xFFFF0203FFFF0001));
        // 00000000 00000000 RRRrrGGG gggBBBbb
        __m128i red = _mm_srli_epi32(colors565, 8);
        __m128i green = _mm_slli_epi32(colors565, 5) & _mm_set1_epi32(0x0000FC00);
        __m128i blue = _mm_slli_epi32(colors565, 19);
        __m128i blue_red = (blue | red) & _mm_set1_epi32(0x00F800F8);
        __m128i copy3 = _mm_srli_epi16(blue_red, 5);
        __m128i copy2 = _mm_srli_epi32(green, 6) & _mm_set1_epi32(0x00000300);
        __m128i alpha = _mm_set1_epi32(0xFF000000);
        __m128i colors8888 = (blue_red | copy3) | (green | copy2) | alpha;

        // Make each color component 16 bits wide.
        __m256i colors16 = _mm256_cvtepu8_epi16(colors8888);
        __m256i c0 = _mm256_shuffle_epi32(colors16, _MM_SHUFFLE(1, 0, 1, 0));
        __m256i c1 = _mm256_shuffle_epi32(colors16, _MM_SHUFFLE(3, 2, 3, 2));

        // Calculate c2 and c3 for the case c0 > c1.
        __m256i diff = _mm256_sub_epi16(c1, c0);
        __m256i diff2 = _mm256_srai_epi16(diff, 1);
        __m256i diff8 = _mm256_srai_epi16(diff, 3);
        __m256i delta = _mm256_sub_epi16(diff2, diff8);
        __m256i c2_1 = _mm256_add_epi16(colors16, delta);
        __m256i c3_1 = _mm256_sub_epi16(colors16, delta);

        // Calculate c2 and c3 for the case c0 <= c1.
        __m256i c2_0 = _mm256_avg_epu16(c1, c0);
        __m256i c3_0 = c2_0 & _mm256_set1_epi64x(0x0000FFFFFFFFFFFF);

        __m256i c32_0 = _mm256_blend_epi16(c3_0, c2_0, 0b00001111);
        __m256i c32_1 = _mm256_blend_epi16(c3_1, c2_1, 0b00001111);

        // Convert to the correct lexicographical ordering (RGB) for the
        // comparison.
        // TODO: Maybe there is a better way?
        __m256i rgb_order = _mm256_set_epi64x(0xFFFF09080B0A0D0C, 0xFFFF010003020504,
                                              0xFFFF09080B0A0D0C, 0xFFFF010003020504);
        c0 = _mm256_shuffle_epi8(c0, rgb_order);
        c1 = _mm256_shuffle_epi8(c1, rgb_order);

        __m256i cmp = _mm256_cmpgt_epi64(c0, c1);
        __m256i c32 = _mm256_blendv_epi8(c32_0, c32_1, cmp);
        __m256i palette = _mm256_packus_epi16(colors16, c32);

        // Expand 2-bit indices to bytes (premultiplied by 4).
        u64 pos = 0x0303030303030303u * 4;
        u64 i0 = _pdep_u64(pdxt[0].indices >> 0, pos);
        u64 i1 = _pdep_u64(pdxt[0].indices >> 16, pos);
        u64 i2 = _pdep_u64(pdxt[1].indices >> 0, pos);
        u64 i3 = _pdep_u64(pdxt[1].indices >> 16, pos);
        __m256i index_bytes = _mm256_set_epi64x(i3, i2, i1, i0);
        __m256i index_shuffle = _mm256_set_epi32(0x00000000, 0x01010101, 0x02020202, 0x03030303,
                                                 0x00000000, 0x01010101, 0x02020202, 0x03030303);

        // For each line of 8 pixels from two DXT blocks:
        for (int i = 0; i < 4; i++)
        {
          // Select, swap, and quadruplicate 8 indices for this line.
          __m256i current_indices = _mm256_shuffle_epi8(index_bytes, index_shuffle);
          // Now turn the premultiplied color indices into byte indices.
          current_indices |= _mm256_set1_epi32(0x03020100);
          // Use indices to select colors from the palette and store them.
          __m256i result = _mm256_shuffle_epi8(palette, current_indices);
          _mm256_store_si256((__m256i*)&dst32[i * width], result);
          // Update the shuffle mask to reference the indices of the next line.
          index_shuffle = _mm256_add_epi8(index_shuffle, _mm256_set1_epi8(4));
        }
        // IACA_END
      }
    }
  }
}

static void InitSSE2()
{
  g_decoder_funcs[GX_TF_I4] = DecodeTexture<8, 8, 4, DecodeBytes_I4_SSE2, 4>;
  g_decoder_funcs[GX_TF_I8] = DecodeTexture<8, 4, 8, DecodeBytes_I8_SSE2>;
  g_decoder_funcs[GX_TF_IA4] = DecodeTexture<8, 4, 8, DecodeBytes_IA4_SSE2, 2>;
  g_decoder_funcs[GX_TF_IA8] = DecodeTexture<4, 4, 8, DecodeBytes_IA8_SSE2, 2>;
  g_decoder_funcs[GX_TF_RGB565] = DecodeTexture<4, 4, 8, DecodeBytes_RGB565_SSE2>;
  g_decoder_funcs[GX_TF_RGB5A3] = DecodeTexture<4, 4, 8, DecodeBytes_RGB5A3_SSE2, 2>;
  g_decoder_funcs[GX_TF_RGBA8] = DecodeTexture<4, 4, 16, DecodeBytes_RGBA8_SSE2, 4>;
  g_decoder_funcs[GX_TF_CMPR] = DecodeDXT_SSE2;
}

static void InitSSSE3()
{
  g_decoder_funcs[GX_TF_I4] = DecodeTexture<8, 8, 4, DecodeBytes_I4_SSSE3, 4>;
  g_decoder_funcs[GX_TF_I8] = DecodeTextureSSSE3<8, 4, 8, DecodeBytes_I8_SSSE3>;
  g_decoder_funcs[GX_TF_IA4] = DecodeTextureSSSE3<8, 4, 8, DecodeBytes_IA4_SSSE3>;
  g_decoder_funcs[GX_TF_IA8] = DecodeTextureSSSE3<4, 4, 8, DecodeBytes_IA8_SSSE3>;
  // SSSE3 doesn't add anything useful for RGB565.
  g_decoder_funcs[GX_TF_RGB5A3] = DecodeTextureSSSE3<4, 4, 8, DecodeBytes_RGB5A3_SSSE3>;
  g_decoder_funcs[GX_TF_RGBA8] = DecodeTexture<4, 4, 16, DecodeBytes_RGBA8_SSSE3, 4>;
  // TODO: CMPR
}

static void InitSSE41()
{
  g_decoder_funcs[GX_TF_RGB5A3] = DecodeTextureSSE41<4, 4, 8, DecodeBytes_RGB5A3_SSE41>;
}

static void InitAVX2()
{
  g_decoder_funcs[GX_TF_I4] = DecodeTextureAVX2<8, 8, 4, DecodeBytes_I4_AVX2, 8>;
  g_decoder_funcs[GX_TF_I8] = DecodeTextureAVX2<8, 4, 8, DecodeBytes_I8_AVX2, 2>;
  g_decoder_funcs[GX_TF_IA4] = DecodeTextureAVX2<8, 4, 8, DecodeBytes_IA4_AVX2, 2>;
  // IA8 with AVX2 isn't faster than SSSE3.
  g_decoder_funcs[GX_TF_RGB565] = DecodeTextureAVX2<4, 4, 8, DecodeBytes_RGB565_AVX2, 2>;
  g_decoder_funcs[GX_TF_RGB5A3] = DecodeTextureAVX2<4, 4, 8, DecodeBytes_RGB5A3_AVX2, 2>;
  g_decoder_funcs[GX_TF_RGBA8] = DecodeTextureAVX2<4, 4, 16, DecodeBytes_RGBA8_AVX2, 4>;
  g_decoder_funcs[GX_TF_CMPR] = DecodeDXT_AVX2;
  g_c4_funcs[GX_TL_IA8] = DecodeTextureAVX2<8, 8, 4, DecodeBytes_C4_AVX2<GX_TL_IA8>, 4>;
  g_c4_funcs[GX_TL_RGB565] = DecodeTextureAVX2<8, 8, 4, DecodeBytes_C4_AVX2<GX_TL_RGB565>, 4>;
  g_c4_funcs[GX_TL_RGB5A3] = DecodeTextureAVX2<8, 8, 4, DecodeBytes_C4_AVX2<GX_TL_RGB5A3>, 4>;
  g_c8_funcs[GX_TL_IA8] = DecodeTextureAVX2<8, 4, 8, DecodeBytes_C8_AVX2<GX_TL_IA8>, 2>;
  g_c8_funcs[GX_TL_RGB565] = DecodeTextureAVX2<8, 4, 8, DecodeBytes_C8_AVX2<GX_TL_RGB565>, 2>;
  g_c8_funcs[GX_TL_RGB5A3] = DecodeTextureAVX2<8, 4, 8, DecodeBytes_C8_AVX2<GX_TL_RGB5A3>, 2>;
  g_c14_funcs[GX_TL_IA8] = DecodeTextureAVX2<4, 4, 8, DecodeBytes_C14_AVX2<GX_TL_IA8>, 2>;
  g_c14_funcs[GX_TL_RGB565] = DecodeTextureAVX2<4, 4, 8, DecodeBytes_C14_AVX2<GX_TL_RGB565>, 2>;
  g_c14_funcs[GX_TL_RGB5A3] = DecodeTextureAVX2<4, 4, 8, DecodeBytes_C14_AVX2<GX_TL_RGB5A3>, 2>;
}

void InitX64()
{
  if (!cpu_info.bSSE2)
    return;
  InitSSE2();
  if (!cpu_info.bSSSE3)
    return;
  InitSSSE3();
  if (!cpu_info.bSSE4_1)
    return;
  InitSSE41();
  if (!cpu_info.bAVX2)
    return;
  InitAVX2();
}

}  // namespace TextureDecoder
