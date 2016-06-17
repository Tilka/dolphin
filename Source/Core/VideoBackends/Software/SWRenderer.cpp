// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <x86intrin.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

alignas(32) static u8 s_xfbColorTexture[2][MAX_XFB_WIDTH * MAX_XFB_HEIGHT * 4];
static int s_currentColorTexture = 0;

void SWRenderer::Init()
{
  s_currentColorTexture = 0;
}

void SWRenderer::Shutdown()
{
  g_Config.bRunning = false;
  UpdateActiveConfig();
}

void SWRenderer::RenderText(const std::string& pstr, int left, int top, u32 color)
{
  SWOGLWindow::s_instance->PrintText(pstr, left, top, color);
}

u8* SWRenderer::GetNextColorTexture()
{
  return s_xfbColorTexture[!s_currentColorTexture];
}

u8* SWRenderer::GetCurrentColorTexture()
{
  return s_xfbColorTexture[s_currentColorTexture];
}

void SWRenderer::SwapColorTexture()
{
  s_currentColorTexture = !s_currentColorTexture;
}

[[gnu::target("avx2")]]  // Haswell
    void
    SWRenderer::UpdateColorTexture(EfbInterface::yuv422_packed* xfb, u32 fbWidth, u32 fbHeight)
{
  if (fbWidth * fbHeight > MAX_XFB_WIDTH * MAX_XFB_HEIGHT)
  {
    ERROR_LOG(VIDEO, "Framebuffer is too large: %ix%i", fbWidth, fbHeight);
    return;
  }

  auto src = (__m256i*)xfb;
  auto dst = (__m256i*)GetNextColorTexture();

  auto src128 = (__m128i*)xfb;
  auto dst128 = (__m128i*)GetNextColorTexture();

  for (u16 y = 0; y < fbHeight; y++)
  {
#if 1
    // 11.3 cycles for 16 pixels on Haswell
    for (u16 x = 0; x < fbWidth; x += 16)
    {
      auto yuv = _mm256_load_si256(src++);
      auto yy = _mm256_shuffle_epi8(yuv, _mm256_set_epi64x(0x0E0E0C0C0A0A0808, 0x0606040402020000,
                                                           0x0E0E0C0C0A0A0808, 0x0606040402020000));
      auto uv = _mm256_shuffle_epi8(yuv, _mm256_set_epi64x(0x0F0D0F0D0B090B09, 0x0705070503010301,
                                                           0x0F0D0F0D0B090B09, 0x0705070503010301));
      auto r = _mm256_maddubs_epi16(uv, _mm256_set1_epi16((-102 * 256) | 0));
      auto g = _mm256_maddubs_epi16(uv, _mm256_set1_epi16((52 * 256) | 25));
      auto b = _mm256_maddubs_epi16(uv, _mm256_set1_epi16((0 * 256) | (-128 & 0xFF)));
      auto a = _mm256_set1_epi8(-1);
      r = _mm256_sub_epi16(_mm256_set1_epi16(-102 * 128 - 1160), r);
      g = _mm256_sub_epi16(_mm256_set1_epi16(25 * 128 + 52 * 128 - 1160), g);
      b = _mm256_sub_epi16(_mm256_set1_epi16(-128 * 128 - 1160), b);
      yy = _mm256_mulhi_epu16(yy, _mm256_set1_epi16(18997));
      r = _mm256_add_epi16(r, yy);
      g = _mm256_add_epi16(g, yy);
      b = _mm256_add_epi16(b, yy);
      r = _mm256_srai_epi16(r, IntLog2(64));
      g = _mm256_srai_epi16(g, IntLog2(64));
      b = _mm256_srai_epi16(b, IntLog2(64));
      r = _mm256_packus_epi16(r, r);
      g = _mm256_packus_epi16(g, g);
      b = _mm256_packus_epi16(b, b);
      auto lo = _mm256_unpacklo_epi8(r, g);
      auto hi = _mm256_unpacklo_epi8(b, a);
      lo = _mm256_permute4x64_epi64(lo, _MM_SHUFFLE(3, 1, 2, 0));
      hi = _mm256_permute4x64_epi64(hi, _MM_SHUFFLE(3, 1, 2, 0));
      _mm256_store_si256(dst++, _mm256_unpacklo_epi16(lo, hi));
      _mm256_store_si256(dst++, _mm256_unpackhi_epi16(lo, hi));
    }
#else
    // SSE2
    for (u16 x = 0; x < fbWidth; x += 8)
    {
      auto yuv422 = _mm_load_si128(src128++);
      __m128i arr[8];
      auto zero = _mm_setzero_si128();
      auto lo = _mm_unpacklo_epi8(yuv422, zero);
      auto hi = _mm_unpackhi_epi8(yuv422, zero);
      __m128i expanded[4] = {
          _mm_unpacklo_epi16(lo, zero), _mm_unpackhi_epi16(lo, zero), _mm_unpacklo_epi16(hi, zero),
          _mm_unpackhi_epi16(hi, zero),
      };
      for (int i = 0; i < 4; i++)
      {
        auto cvt = _mm_cvtepi32_ps(expanded[i]);

        auto y1 = _mm_permute_ps(cvt, _MM_SHUFFLE(0, 0, 0, 0));
        auto uu = _mm_permute_ps(cvt, _MM_SHUFFLE(1, 1, 1, 1));
        auto y2 = _mm_permute_ps(cvt, _MM_SHUFFLE(2, 2, 2, 2));
        auto vv = _mm_permute_ps(cvt, _MM_SHUFFLE(3, 3, 3, 3));

        vv = _mm_mul_ps(vv, _mm_set_ps(0, 0, -0.813f, 1.596f));
        uu = _mm_mul_ps(uu, _mm_set_ps(0, 2.017f, -0.392f, 0));
        y1 = _mm_mul_ps(y1, _mm_set1_ps(1.164f));
        y2 = _mm_mul_ps(y2, _mm_set1_ps(1.164f));
        vv = _mm_add_ps(vv, _mm_set_ps(255, -276.836f, 135.576f, -222.921f));
        uu = _mm_add_ps(uu, vv);
        y1 = _mm_add_ps(y1, uu);
        y2 = _mm_add_ps(y2, uu);

        arr[2 * i + 0] = _mm_cvtps_epi32(y1);
        arr[2 * i + 1] = _mm_cvtps_epi32(y2);
      }
      arr[0] = _mm_packs_epi32(arr[0], arr[1]);
      arr[1] = _mm_packs_epi32(arr[2], arr[3]);
      arr[2] = _mm_packs_epi32(arr[4], arr[5]);
      arr[3] = _mm_packs_epi32(arr[6], arr[7]);
      arr[0] = _mm_packus_epi16(arr[0], arr[1]);
      arr[1] = _mm_packus_epi16(arr[2], arr[3]);
      _mm_store_si128(dst128++, arr[0]);
      _mm_store_si128(dst128++, arr[1]);
    }
#endif
  }
  SwapColorTexture();
}

// Called on the GPU thread
void SWRenderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight,
                          const EFBRectangle& rc, float Gamma)
{
  if (!Fifo::WillSkipCurrentFrame())
  {
    if (g_ActiveConfig.bUseXFB)
    {
      EfbInterface::yuv422_packed* xfb = (EfbInterface::yuv422_packed*)Memory::GetPointer(xfbAddr);
      UpdateColorTexture(xfb, fbWidth, fbHeight);
    }
    else
    {
      EfbInterface::BypassXFB(GetCurrentColorTexture(), fbWidth, fbHeight, rc, Gamma);
    }

    // Save screenshot
    if (s_bScreenshot)
    {
      std::lock_guard<std::mutex> lk(s_criticalScreenshot);

      if (TextureToPng(GetCurrentColorTexture(), fbWidth * 4, s_sScreenshotName, fbWidth, fbHeight,
                       false))
        OSD::AddMessage("Screenshot saved to " + s_sScreenshotName);

      // Reset settings
      s_sScreenshotName.clear();
      s_bScreenshot = false;
      s_screenshotCompleted.Set();
    }

    if (SConfig::GetInstance().m_DumpFrames)
    {
      static int frame_index = 0;
      TextureToPng(GetCurrentColorTexture(), fbWidth * 4,
                   StringFromFormat("%sframe%i_color.png",
                                    File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), frame_index),
                   fbWidth, fbHeight, true);
      frame_index++;
    }
  }

  OSD::DoCallbacks(OSD::CallbackType::OnFrame);

  DrawDebugText();

  SWOGLWindow::s_instance->ShowImage(GetCurrentColorTexture(), fbWidth * 4, fbWidth, fbHeight, 1.0);

  UpdateActiveConfig();

  // virtual XFB is not supported
  if (g_ActiveConfig.bUseXFB)
    g_ActiveConfig.bUseRealXFB = true;
}

u32 SWRenderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
  u32 value = 0;

  switch (type)
  {
  case PEEK_Z:
  {
    value = EfbInterface::GetDepth(x, y);
    break;
  }
  case PEEK_COLOR:
  {
    u32 color = 0;
    EfbInterface::GetColor(x, y, (u8*)&color);

    // rgba to argb
    value = (color >> 8) | (color & 0xff) << 24;
    break;
  }
  default:
    break;
  }

  return value;
}

u16 SWRenderer::BBoxRead(int index)
{
  return BoundingBox::coords[index];
}

void SWRenderer::BBoxWrite(int index, u16 value)
{
  BoundingBox::coords[index] = value;
}

TargetRectangle SWRenderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
  TargetRectangle result;
  result.left = rc.left;
  result.top = rc.top;
  result.right = rc.right;
  result.bottom = rc.bottom;
  return result;
}

void SWRenderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable,
                             bool zEnable, u32 color, u32 z)
{
  EfbCopy::ClearEfb();
}
