// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/TextureDecoder_Internal.h"
#include "VideoCommon/sfont.inc"

static bool TexFmt_Overlay_Enable = false;
static bool TexFmt_Overlay_Center = false;

// TRAM
// STATE_TO_SAVE
alignas(32) u8 texMem[TMEM_SIZE];

namespace TextureDecoder
{
int GetTexelSizeInNibbles(int format)
{
  switch (format & 0x3f)
  {
  case GX_TF_I4:
    return 1;
  case GX_TF_I8:
    return 2;
  case GX_TF_IA4:
    return 2;
  case GX_TF_IA8:
    return 4;
  case GX_TF_RGB565:
    return 4;
  case GX_TF_RGB5A3:
    return 4;
  case GX_TF_RGBA8:
    return 8;
  case GX_TF_C4:
    return 1;
  case GX_TF_C8:
    return 2;
  case GX_TF_C14X2:
    return 4;
  case GX_TF_CMPR:
    return 1;
  case GX_CTF_R4:
    return 1;
  case GX_CTF_RA4:
    return 2;
  case GX_CTF_RA8:
    return 4;
  case GX_CTF_A8:
    return 2;
  case GX_CTF_R8:
    return 2;
  case GX_CTF_G8:
    return 2;
  case GX_CTF_B8:
    return 2;
  case GX_CTF_RG8:
    return 4;
  case GX_CTF_GB8:
    return 4;

  case GX_TF_Z8:
    return 2;
  case GX_TF_Z16:
    return 4;
  case GX_TF_Z24X8:
    return 8;

  case GX_CTF_Z4:
    return 1;
  case GX_CTF_Z8H:
    return 2;
  case GX_CTF_Z8M:
    return 2;
  case GX_CTF_Z8L:
    return 2;
  case GX_CTF_Z16R:
    return 4;
  case GX_CTF_Z16L:
    return 4;
  default:
    PanicAlert("Unsupported Texture Format (%08x)! (GetTexelSizeInNibbles)", format);
    return 1;
  }
}

int GetTextureSizeInBytes(int width, int height, int format)
{
  return (width * height * GetTexelSizeInNibbles(format) + 1) / 2;
}

int GetBlockWidthInTexels(u32 format)
{
  switch (format)
  {
  case GX_TF_I4:
    return 8;
  case GX_TF_I8:
    return 8;
  case GX_TF_IA4:
    return 8;
  case GX_TF_IA8:
    return 4;
  case GX_TF_RGB565:
    return 4;
  case GX_TF_RGB5A3:
    return 4;
  case GX_TF_RGBA8:
    return 4;
  case GX_TF_C4:
    return 8;
  case GX_TF_C8:
    return 8;
  case GX_TF_C14X2:
    return 4;
  case GX_TF_CMPR:
    return 8;
  case GX_CTF_R4:
    return 8;
  case GX_CTF_RA4:
    return 8;
  case GX_CTF_RA8:
    return 4;
  case GX_CTF_A8:
    return 8;
  case GX_CTF_R8:
    return 8;
  case GX_CTF_G8:
    return 8;
  case GX_CTF_B8:
    return 8;
  case GX_CTF_RG8:
    return 4;
  case GX_CTF_GB8:
    return 4;
  case GX_TF_Z8:
    return 8;
  case GX_TF_Z16:
    return 4;
  case GX_TF_Z24X8:
    return 4;
  case GX_CTF_Z4:
    return 8;
  case GX_CTF_Z8H:
    return 8;
  case GX_CTF_Z8M:
    return 8;
  case GX_CTF_Z8L:
    return 8;
  case GX_CTF_Z16R:
    return 4;
  case GX_CTF_Z16L:
    return 4;
  default:
    PanicAlert("Unsupported Texture Format (%08x)! (GetBlockWidthInTexels)", format);
    return 8;
  }
}

int GetBlockHeightInTexels(u32 format)
{
  switch (format)
  {
  case GX_TF_I4:
    return 8;
  case GX_TF_I8:
    return 4;
  case GX_TF_IA4:
    return 4;
  case GX_TF_IA8:
    return 4;
  case GX_TF_RGB565:
    return 4;
  case GX_TF_RGB5A3:
    return 4;
  case GX_TF_RGBA8:
    return 4;
  case GX_TF_C4:
    return 8;
  case GX_TF_C8:
    return 4;
  case GX_TF_C14X2:
    return 4;
  case GX_TF_CMPR:
    return 8;
  case GX_CTF_R4:
    return 8;
  case GX_CTF_RA4:
    return 4;
  case GX_CTF_RA8:
    return 4;
  case GX_CTF_A8:
    return 4;
  case GX_CTF_R8:
    return 4;
  case GX_CTF_G8:
    return 4;
  case GX_CTF_B8:
    return 4;
  case GX_CTF_RG8:
    return 4;
  case GX_CTF_GB8:
    return 4;
  case GX_TF_Z8:
    return 4;
  case GX_TF_Z16:
    return 4;
  case GX_TF_Z24X8:
    return 4;
  case GX_CTF_Z4:
    return 8;
  case GX_CTF_Z8H:
    return 4;
  case GX_CTF_Z8M:
    return 4;
  case GX_CTF_Z8L:
    return 4;
  case GX_CTF_Z16R:
    return 4;
  case GX_CTF_Z16L:
    return 4;
  default:
    PanicAlert("Unsupported Texture Format (%08x)! (GetBlockHeightInTexels)", format);
    return 4;
  }
}

// returns bytes
int GetPaletteSize(int format)
{
  switch (format)
  {
  case GX_TF_C4:
    return 16 * 2;
  case GX_TF_C8:
    return 256 * 2;
  case GX_TF_C14X2:
    return 16384 * 2;
  default:
    return 0;
  }
}

// Get the "in memory" texture format of an EFB copy's format.
// With the exception of c4/c8/c14 paletted texture formats (which are handled elsewhere)
// this is the format the game should be using when it is drawing an EFB copy back.
int GetEfbCopyBaseFormat(int format)
{
  switch (format)
  {
  case GX_TF_I4:
  case GX_CTF_Z4:
  case GX_CTF_R4:
    return GX_TF_I4;
  case GX_TF_I8:
  case GX_CTF_A8:
  case GX_CTF_R8:
  case GX_CTF_G8:
  case GX_CTF_B8:
  case GX_TF_Z8:
  case GX_CTF_Z8H:
  case GX_CTF_Z8M:
  case GX_CTF_Z8L:
    return GX_TF_I8;
  case GX_TF_IA4:
  case GX_CTF_RA4:
    return GX_TF_IA4;
  case GX_TF_IA8:
  case GX_TF_Z16:
  case GX_CTF_RA8:
  case GX_CTF_RG8:
  case GX_CTF_GB8:
  case GX_CTF_Z16R:
  case GX_CTF_Z16L:
    return GX_TF_IA8;
  case GX_TF_RGB565:
    return GX_TF_RGB565;
  case GX_TF_RGB5A3:
    return GX_TF_RGB5A3;
  case GX_TF_RGBA8:
  case GX_TF_Z24X8:
  case GX_CTF_YUVA8:
    return GX_TF_RGBA8;
  // These formats can't be (directly) generated by EFB copies
  case GX_TF_C4:
  case GX_TF_C8:
  case GX_TF_C14X2:
  case GX_TF_CMPR:
  default:
    PanicAlert("Unsupported Texture Format (%08x)! (GetEfbCopyBaseFormat)", format);
    return format & 0xf;
  }
}

void SetTexFmtOverlayOptions(bool enable, bool center)
{
  TexFmt_Overlay_Enable = enable;
  TexFmt_Overlay_Center = center;
}

const char* GetTextureFormatName(int format)
{
  static const char* texfmt[] = {
      // pixel
      "I4", "I8", "IA4", "IA8", "RGB565", "RGB5A3", "RGBA8", "0x07", "C4", "C8", "C14X2", "0x0B",
      "0x0C", "0x0D", "CMPR", "0x0F",
      // Z-buffer
      "0x10", "Z8", "0x12", "Z16", "0x14", "0x15", "Z24X8", "0x17", "0x18", "0x19", "0x1A", "0x1B",
      "0x1C", "0x1D", "0x1E", "0x1F",
      // pixel + copy
      "CR4", "0x21", "CRA4", "CRA8", "0x24", "0x25", "CYUVA8", "CA8", "CR8", "CG8", "CB8", "CRG8",
      "CGB8", "0x2D", "0x2E", "0x2F",
      // Z + copy
      "CZ4", "0x31", "0x32", "0x33", "0x34", "0x35", "0x36", "0x37", "0x38", "CZ8M", "CZ8L", "0x3B",
      "CZ16L", "0x3D", "0x3E", "0x3F",
  };

  return texfmt[format];
}

const char* GetTlutFormatName(TlutFormat format)
{
  static const char* tlut_format[] = {"IA8", "RGB565", "RGB5A3", "0x03"};

  return tlut_format[format];
}

static void DrawOverlay(u32* dst, int width, int height, int texformat)
{
  int w = std::min(width, 40);
  int h = std::min(height, 10);

  int xoff = (width - w) >> 1;
  int yoff = (height - h) >> 1;

  if (!TexFmt_Overlay_Center)
  {
    xoff = 0;
    yoff = 0;
  }

  const char* fmt = GetTextureFormatName(texformat & 0xF);
  while (*fmt)
  {
    int xcnt = 0;
    int nchar = sfont_map[(int)*fmt];

    const unsigned char* ptr = sfont_raw[nchar];  // each char is up to 9x10

    for (int x = 0; x < 9; x++)
    {
      if (ptr[x] == 0x78)
        break;
      xcnt++;
    }

    for (int y = 0; y < 10; y++)
    {
      for (int x = 0; x < xcnt; x++)
      {
        dst[(y + yoff) * width + x + xoff] = ptr[x] ? 0xFFFFFFFF : 0xFF000000;
      }
      ptr += 9;
    }
    xoff += xcnt;
    fmt++;
  }
}

// One function pointer for each texture format.
DecodeFunction* g_decoder_funcs[15];
DecodeFunction* g_c4_funcs[3];
DecodeFunction* g_c8_funcs[3];
DecodeFunction* g_c14_funcs[3];

static TlutFormat s_tlutfmt;

static void DecodeC4(u32* dst, const u8* src, const u16* tlut, int width, int height)
{
  g_c4_funcs[s_tlutfmt](dst, src, tlut, width, height);
}

static void DecodeC8(u32* dst, const u8* src, const u16* tlut, int width, int height)
{
  g_c8_funcs[s_tlutfmt](dst, src, tlut, width, height);
}

static void DecodeC14(u32* dst, const u8* src, const u16* tlut, int width, int height)
{
  g_c14_funcs[s_tlutfmt](dst, src, tlut, width, height);
}

void Init()
{
  g_decoder_funcs[GX_TF_C4] = DecodeC4;
  g_decoder_funcs[GX_TF_C8] = DecodeC8;
  g_decoder_funcs[GX_TF_C14X2] = DecodeC14;
  InitGeneric();
#ifdef _M_X86
  InitX64();
#endif
}

void Decode(u8* dst, const u8* src, int width, int height, int texformat, const u16* tlut,
            TlutFormat tlutfmt)
{
  u32* dst32 = reinterpret_cast<u32*>(dst);
  s_tlutfmt = tlutfmt;
  g_decoder_funcs[texformat](dst32, src, tlut, width, height);

  if (TexFmt_Overlay_Enable)
    DrawOverlay(dst32, width, height, texformat);
}

static inline u32 DecodePixel_Paletted(u16 pixel, TlutFormat tlutfmt)
{
  pixel = Common::swap16(pixel);
  switch (tlutfmt)
  {
  case GX_TL_IA8:
    return DecodePixel_IA8(pixel);
  case GX_TL_RGB565:
    return DecodePixel_RGB565(pixel);
  case GX_TL_RGB5A3:
    return DecodePixel_RGB5A3(pixel);
  default:
    return 0;
  }
}

struct DXTBlock
{
  u16 color1;
  u16 color2;
  u8 lines[4];
};

void DecodeTexel(u8* dst, const u8* src, int s, int t, int imageWidth, int texformat,
                 const u16* tlut, TlutFormat tlutfmt)
{
  /* General formula for computing texture offset
  //
  u16 sBlk = s / blockWidth;
  u16 tBlk = t / blockHeight;
  u16 widthBlks = (width / blockWidth) + 1;
  u32 base = (tBlk * widthBlks + sBlk) * blockWidth * blockHeight;
  u16 blkS = s & (blockWidth - 1);
  u16 blkT =  t & (blockHeight - 1);
  u32 blkOff = blkT * blockWidth + blkS;
  */
  u32* dst32 = (u32*)dst;

  switch (texformat)
  {
  case GX_TF_C4:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 3;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 7;
    u32 blkOff = (blkT << 3) + blkS;

    int rs = (blkOff & 1) ? 0 : 4;
    u32 offset = base + (blkOff >> 1);

    u8 val = (*(src + offset) >> rs) & 0xF;

    *dst32 = DecodePixel_Paletted(tlut[val], tlutfmt);
  }
  break;
  case GX_TF_I4:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 3;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 7;
    u32 blkOff = (blkT << 3) + blkS;

    int rs = (blkOff & 1) ? 0 : 4;
    u32 offset = base + (blkOff >> 1);

    u8 val = (*(src + offset) >> rs) & 0xF;
    val = Convert4To8(val);
    dst[0] = val;
    dst[1] = val;
    dst[2] = val;
    dst[3] = val;
  }
  break;
  case GX_TF_I8:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 3) + blkS;

    u8 val = *(src + base + blkOff);
    dst[0] = val;
    dst[1] = val;
    dst[2] = val;
    dst[3] = val;
  }
  break;
  case GX_TF_C8:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 3) + blkS;

    u8 val = *(src + base + blkOff);

    *dst32 = DecodePixel_Paletted(tlut[val], tlutfmt);
  }
  break;
  case GX_TF_IA4:
  {
    u16 sBlk = s >> 3;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;
    u16 blkS = s & 7;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 3) + blkS;

    u8 val = *(src + base + blkOff);
    const u8 a = Convert4To8(val >> 4);
    const u8 l = Convert4To8(val & 0xF);
    dst[0] = l;
    dst[1] = l;
    dst[2] = l;
    dst[3] = a;
  }
  break;
  case GX_TF_IA8:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    const u16* valAddr = (u16*)(src + offset);

    *dst32 = DecodePixel_IA8(*valAddr);
  }
  break;
  case GX_TF_C14X2:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    const u16* valAddr = (u16*)(src + offset);

    u16 val = Common::swap16(*valAddr) & 0x3FFF;

    *dst32 = DecodePixel_Paletted(tlut[val], tlutfmt);
  }
  break;
  case GX_TF_RGB565:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    const u16* valAddr = (u16*)(src + offset);

    *dst32 = DecodePixel_RGB565(Common::swap16(*valAddr));
  }
  break;
  case GX_TF_RGB5A3:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 4;
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    const u16* valAddr = (u16*)(src + offset);

    *dst32 = DecodePixel_RGB5A3(Common::swap16(*valAddr));
  }
  break;
  case GX_TF_RGBA8:
  {
    u16 sBlk = s >> 2;
    u16 tBlk = t >> 2;
    u16 widthBlks = (imageWidth >> 2) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 5;  // shift by 5 is correct
    u16 blkS = s & 3;
    u16 blkT = t & 3;
    u32 blkOff = (blkT << 2) + blkS;

    u32 offset = (base + blkOff) << 1;
    const u8* valAddr = src + offset;

    dst[3] = valAddr[0];
    dst[0] = valAddr[1];
    dst[1] = valAddr[32];
    dst[2] = valAddr[33];
  }
  break;
  case GX_TF_CMPR:
  {
    u16 sDxt = s >> 2;
    u16 tDxt = t >> 2;

    u16 sBlk = sDxt >> 1;
    u16 tBlk = tDxt >> 1;
    u16 widthBlks = (imageWidth >> 3) + 1;
    u32 base = (tBlk * widthBlks + sBlk) << 2;
    u16 blkS = sDxt & 1;
    u16 blkT = tDxt & 1;
    u32 blkOff = (blkT << 1) + blkS;

    u32 offset = (base + blkOff) << 3;

    const DXTBlock* dxtBlock = (const DXTBlock*)(src + offset);

    u16 c1 = Common::swap16(dxtBlock->color1);
    u16 c2 = Common::swap16(dxtBlock->color2);
    int blue1 = Convert5To8(c1 & 0x1F);
    int blue2 = Convert5To8(c2 & 0x1F);
    int green1 = Convert6To8((c1 >> 5) & 0x3F);
    int green2 = Convert6To8((c2 >> 5) & 0x3F);
    int red1 = Convert5To8((c1 >> 11) & 0x1F);
    int red2 = Convert5To8((c2 >> 11) & 0x1F);

    // Approximation of x/3: 3/8 (1/2 - 1/8)
    int blue3 = ((blue2 - blue1) >> 1) - ((blue2 - blue1) >> 3);
    int green3 = ((green2 - green1) >> 1) - ((green2 - green1) >> 3);
    int red3 = ((red2 - red1) >> 1) - ((red2 - red1) >> 3);

    u16 ss = s & 3;
    u16 tt = t & 3;

    int colorSel = dxtBlock->lines[tt];
    int rs = 6 - (ss << 1);
    colorSel = (colorSel >> rs) & 3;
    colorSel |= c1 > c2 ? 0 : 4;

    u32 color = 0;

    switch (colorSel)
    {
    case 0:
    case 4:
      color = MakeRGBA(red1, green1, blue1, 255);
      break;
    case 1:
    case 5:
      color = MakeRGBA(red2, green2, blue2, 255);
      break;
    case 2:
      color = MakeRGBA(red1 + red3, green1 + green3, blue1 + blue3, 255);
      break;
    case 3:
      color = MakeRGBA(red2 - red3, green2 - green3, blue2 - blue3, 255);
      break;
    case 6:
      color =
          MakeRGBA((red1 + red2 + 1) / 2, (green1 + green2 + 1) / 2, (blue1 + blue2 + 1) / 2, 255);
      break;
    case 7:
      // color[3] is the same as color[2] (average of both colors), but transparent.
      // This differs from DXT1 where color[3] is transparent black.
      color =
          MakeRGBA((red1 + red2 + 1) / 2, (green1 + green2 + 1) / 2, (blue1 + blue2 + 1) / 2, 0);
      break;
    default:
      color = 0;
      break;
    }

    *dst32 = color;
  }
  break;
  }
}

void DecodeTexelRGBA8FromTmem(u8* dst, const u8* src_ar, const u8* src_gb, int s, int t,
                              int imageWidth)
{
  u16 sBlk = s >> 2;
  u16 tBlk = t >> 2;
  u16 widthBlks =
      (imageWidth >> 2) + 1;  // TODO: Looks wrong. Shouldn't this be ((imageWidth-1)>>2)+1 ?
  u32 base_ar = (tBlk * widthBlks + sBlk) << 4;
  u32 base_gb = (tBlk * widthBlks + sBlk) << 4;
  u16 blkS = s & 3;
  u16 blkT = t & 3;
  u32 blk_off = (blkT << 2) + blkS;

  u32 offset_ar = (base_ar + blk_off) << 1;
  u32 offset_gb = (base_gb + blk_off) << 1;
  const u8* val_addr_ar = src_ar + offset_ar;
  const u8* val_addr_gb = src_gb + offset_gb;

  dst[3] = val_addr_ar[0];  // A
  dst[0] = val_addr_ar[1];  // R
  dst[1] = val_addr_gb[0];  // G
  dst[2] = val_addr_gb[1];  // B
}

void DecodeRGBA8FromTmem(u8* dst, const u8* src_ar, const u8* src_gb, int width, int height)
{
  // TODO for someone who cares: Make this less slow!
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      DecodeTexelRGBA8FromTmem(dst, src_ar, src_gb, x, y, width - 1);
      dst += 4;
    }
  }
}

}  // namespace TextureDecoder
