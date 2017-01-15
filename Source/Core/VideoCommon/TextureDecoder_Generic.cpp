// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/CommonTypes.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureDecoder_Internal.h"
#include "VideoCommon/VideoConfig.h"

// GameCube/Wii texture decoder

namespace TextureDecoder
{
static void DecodeBytes_I4_Generic(u32* dst, const u8* src, const u16*, int)
{
  for (int i = 0; i < 4; i++)
  {
    u32 c0 = Convert4To8(src[i] >> 4);
    u32 c1 = Convert4To8(src[i] & 0xF);
    dst[2 * i + 0] = c0 << 24 | c0 << 16 | c0 << 8 | c0;
    dst[2 * i + 1] = c1 << 24 | c1 << 16 | c1 << 8 | c1;
  }
}

static void DecodeBytes_I8_Generic(u32* dst, const u8* src, const u16*, int)
{
  for (int i = 0; i < 8; i++)
  {
    u32 c = src[i];
    dst[i] = c << 24 | c << 16 | c << 8 | c;
  }
}

static void DecodeBytes_IA4_Generic(u32* dst, const u8* src, const u16*, int)
{
  for (int x = 0; x < 8; x++)
  {
    u32 a = Convert4To8(src[x] >> 4);
    u32 i = Convert4To8(src[x] & 0xF);
    dst[x] = a << 24 | i << 16 | i << 8 | i;
  }
}

static void DecodeBytes_IA8_Generic(u32* dst, const u8* src, const u16*, int)
{
  const u16* src16 = (u16*)src;
  for (int x = 0; x < 4; x++)
    dst[x] = DecodePixel<GX_TL_IA8>(src16[x]);
}

static void DecodeBytes_RGB565_Generic(u32* dst, const u8* src, const u16*, int)
{
  u16* src16 = (u16*)src;
  for (int i = 0; i < 4; i++)
    dst[i] = DecodePixel<GX_TL_RGB565>(src16[i]);
}

static void DecodeBytes_RGB5A3_Generic(u32* dst, const u8* src, const u16*, int)
{
  u16* src16 = (u16*)src;
  for (int i = 0; i < 4; i++)
    dst[i] = DecodePixel<GX_TL_RGB5A3>(src16[i]);
}

static void DecodeBytes_RGBA8_Generic(u32* dst, const u8* src, const u16*, int width)
{
  for (int iy = 0; iy < 4; iy++, src += 8)
  {
    u16* s0 = (u16*)src;
    u16* s1 = (u16*)(src + 32);
    u32* d = dst + iy * width;
    for (int ix = 0; ix < 4; ix++)
      d[ix] = ((s0[ix] & 0xFF) << 24) | ((s0[ix] & 0xFF00) >> 8) | (s1[ix] << 8);
  }
}

template <TlutFormat format>
static inline void DecodeBytes_C4_Generic(u32* dst, const u8* src, const u16* tlut, int)
{
  for (int x = 0; x < 4; x++)
  {
    u8 val = src[x];
    *dst++ = DecodePixel<format>(tlut[val >> 4]);
    *dst++ = DecodePixel<format>(tlut[val & 0xF]);
  }
}

template <TlutFormat format>
static inline void DecodeBytes_C8_Generic(u32* dst, const u8* src, const u16* tlut, int)
{
  for (int x = 0; x < 8; x++)
    *dst++ = DecodePixel<format>(tlut[src[x]]);
}

template <TlutFormat format>
static inline void DecodeBytes_C14X2_Generic(u32* dst, const u8* src_, const u16* tlut, int)
{
  const u16* src = (u16*)src_;
  for (int x = 0; x < 4; x++)
  {
    u16 val = Common::swap16(src[x]);
    *dst++ = DecodePixel<format>(tlut[val & 0x3FFF]);
  }
}

struct DXTBlock
{
  u16 color1;
  u16 color2;
  u8 lines[4];
};

static void DecodeDXTBlock(u32* dst, const DXTBlock* src, int pitch)
{
  u16 c1 = Common::swap16(src->color1);
  u16 c2 = Common::swap16(src->color2);
  int red1 = Convert5To8((c1 >> 11) & 0x1F);
  int red2 = Convert5To8((c2 >> 11) & 0x1F);
  int green1 = Convert6To8((c1 >> 5) & 0x3F);
  int green2 = Convert6To8((c2 >> 5) & 0x3F);
  int blue1 = Convert5To8(c1 & 0x1F);
  int blue2 = Convert5To8(c2 & 0x1F);
  int colors[4];
  colors[0] = MakeRGBA(red1, green1, blue1, 255);
  colors[1] = MakeRGBA(red2, green2, blue2, 255);
  if (c1 > c2)
  {
    // Approximation of x/3: 3/8 (1/2 - 1/8)
    int blue3 = ((blue2 - blue1) >> 1) - ((blue2 - blue1) >> 3);
    int green3 = ((green2 - green1) >> 1) - ((green2 - green1) >> 3);
    int red3 = ((red2 - red1) >> 1) - ((red2 - red1) >> 3);
    colors[2] = MakeRGBA(red1 + red3, green1 + green3, blue1 + blue3, 255);
    colors[3] = MakeRGBA(red2 - red3, green2 - green3, blue2 - blue3, 255);
  }
  else
  {
    // color[3] is the same as color[2] (average of both colors), but transparent.
    // This differs from DXT1 where color[3] is transparent black.
    colors[2] =
        MakeRGBA((red1 + red2 + 1) / 2, (green1 + green2 + 1) / 2, (blue1 + blue2 + 1) / 2, 255);
    colors[3] =
        MakeRGBA((red1 + red2 + 1) / 2, (green1 + green2 + 1) / 2, (blue1 + blue2 + 1) / 2, 0);
  }

  for (int y = 0; y < 4; y++)
  {
    int val = src->lines[y];
    for (int x = 0; x < 4; x++)
    {
      dst[x] = colors[(val >> 6) & 3];
      val <<= 2;
    }
    dst += pitch;
  }
}

static void DecodeDXT_Generic(u32* dst, const u8* src, const u16*, int width, int height)
{
  for (int y = 0; y < height; y += 8)
  {
    for (int x = 0; x < width; x += 8)
    {
      DecodeDXTBlock(dst + y * width + x, (DXTBlock*)src, width);
      src += sizeof(DXTBlock);
      DecodeDXTBlock(dst + y * width + x + 4, (DXTBlock*)src, width);
      src += sizeof(DXTBlock);
      DecodeDXTBlock(dst + (y + 4) * width + x, (DXTBlock*)src, width);
      src += sizeof(DXTBlock);
      DecodeDXTBlock(dst + (y + 4) * width + x + 4, (DXTBlock*)src, width);
      src += sizeof(DXTBlock);
    }
  }
}

void InitGeneric()
{
  g_decoder_funcs[GX_TF_I4] = DecodeTexture<8, 8, 4, DecodeBytes_I4_Generic>;
  g_decoder_funcs[GX_TF_I8] = DecodeTexture<8, 4, 8, DecodeBytes_I8_Generic>;
  g_decoder_funcs[GX_TF_IA4] = DecodeTexture<8, 4, 8, DecodeBytes_IA4_Generic>;
  g_decoder_funcs[GX_TF_IA8] = DecodeTexture<4, 4, 8, DecodeBytes_IA8_Generic>;
  g_decoder_funcs[GX_TF_RGB565] = DecodeTexture<4, 4, 8, DecodeBytes_RGB565_Generic>;
  g_decoder_funcs[GX_TF_RGB5A3] = DecodeTexture<4, 4, 8, DecodeBytes_RGB5A3_Generic>;
  g_decoder_funcs[GX_TF_RGBA8] = DecodeTexture<4, 4, 16, DecodeBytes_RGBA8_Generic, 4>;
  g_decoder_funcs[GX_TF_CMPR] = DecodeDXT_Generic;
  g_c4_funcs[GX_TL_IA8] = DecodeTexture<8, 8, 4, DecodeBytes_C4_Generic<GX_TL_IA8>>;
  g_c4_funcs[GX_TL_RGB565] = DecodeTexture<8, 8, 4, DecodeBytes_C4_Generic<GX_TL_RGB565>>;
  g_c4_funcs[GX_TL_RGB5A3] = DecodeTexture<8, 8, 4, DecodeBytes_C4_Generic<GX_TL_RGB5A3>>;
  g_c8_funcs[GX_TL_IA8] = DecodeTexture<8, 4, 8, DecodeBytes_C8_Generic<GX_TL_IA8>>;
  g_c8_funcs[GX_TL_RGB565] = DecodeTexture<8, 4, 8, DecodeBytes_C8_Generic<GX_TL_RGB565>>;
  g_c8_funcs[GX_TL_RGB5A3] = DecodeTexture<8, 4, 8, DecodeBytes_C8_Generic<GX_TL_RGB5A3>>;
  g_c14_funcs[GX_TL_IA8] = DecodeTexture<4, 4, 8, DecodeBytes_C14X2_Generic<GX_TL_IA8>>;
  g_c14_funcs[GX_TL_RGB565] = DecodeTexture<4, 4, 8, DecodeBytes_C14X2_Generic<GX_TL_RGB565>>;
  g_c14_funcs[GX_TL_RGB5A3] = DecodeTexture<4, 4, 8, DecodeBytes_C14X2_Generic<GX_TL_RGB5A3>>;
}

}  // namespace TextureDecoder
