// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

namespace TextureDecoder
{
using BlockLineDecoder = void(u32* dst, const u8* src, const u16* tlut, int width);

template <int block_width, int block_height, int bytes_per_block_line,
          BlockLineDecoder* decode_block_line, int block_lines_per_call = 1>
__forceinline static void DecodeTexture(u32* __restrict dst, const u8* src, const u16* tlut,
                                        int width, int height)
{
  for (int y = 0, ofs = 0; y < height; y += block_height)
  {
    for (int x = 0; x < width; x += block_width)
    {
      for (int iy = 0; iy < block_height;
           iy += block_lines_per_call, ofs += block_lines_per_call * bytes_per_block_line)
      {
        decode_block_line(dst + (y + iy) * width + x, src + ofs, tlut, width);
      }
    }
  }
}

using DecodeFunction = void(u32* dst, const u8* src, const u16* tlut, int width, int height);
extern DecodeFunction* g_decoder_funcs[15];
extern DecodeFunction* g_c4_funcs[3];
extern DecodeFunction* g_c8_funcs[3];
extern DecodeFunction* g_c14_funcs[3];

void InitGeneric();
#ifdef _M_X86
void InitX64();
#endif

static inline u32 DecodePixel_IA8(u16 val)
{
  int a = val >> 8;
  int i = val & 0xFF;
  return (a << 24) | (i << 16) | (i << 8) | i;
}

static inline u32 DecodePixel_RGB565(u16 val)
{
  int r, g, b, a;
  r = Convert5To8((val >> 11) & 0x1f);
  g = Convert6To8((val >> 5) & 0x3f);
  b = Convert5To8((val)&0x1f);
  a = 0xFF;
  return (a << 24) | (b << 16) | (g << 8) | r;
}

static inline u32 DecodePixel_RGB5A3(u16 val)
{
  int r, g, b, a;
  if (val & 0x8000)
  {
    r = Convert5To8((val >> 10) & 0x1f);
    g = Convert5To8((val >> 5) & 0x1f);
    b = Convert5To8((val)&0x1f);
    a = 0xFF;
  }
  else
  {
    a = Convert3To8((val >> 12) & 0x7);
    r = Convert4To8((val >> 8) & 0xf);
    g = Convert4To8((val >> 4) & 0xf);
    b = Convert4To8((val)&0xf);
  }
  return (a << 24) | (b << 16) | (g << 8) | r;
}

template <TlutFormat format>
static u32 DecodePixel(u16 val)
{
  val = Common::swap16(val);
  switch (format)
  {
  case GX_TL_IA8:
    return DecodePixel_IA8(val);
  case GX_TL_RGB565:
    return DecodePixel_RGB565(val);
  case GX_TL_RGB5A3:
    return DecodePixel_RGB5A3(val);
  default:
    return 0;
  }
}

static inline u32 MakeRGBA(int r, int g, int b, int a)
{
  return a << 24 | b << 16 | g << 8 | r;
}

}  // namespace TextureDecoder
