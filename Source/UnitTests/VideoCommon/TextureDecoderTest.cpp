// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <gtest/gtest.h>

#include "Common/CPUDetect.h"
#include "Common/CommonFuncs.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "VideoCommon/TextureDecoder.h"

enum class CPU
{
  Generic,
  SSE2,
  SSSE3,
  SSE41,
  AVX2,
};

std::ostream& operator<<(std::ostream& os, const CPU& value)
{
  switch (value)
  {
  case CPU::Generic:
    os << "Generic";
    break;
  case CPU::SSE2:
    os << "SSE2";
    break;
  case CPU::SSSE3:
    os << "SSSE3";
    break;
  case CPU::SSE41:
    os << "SSE41";
    break;
  case CPU::AVX2:
    os << "AVX2";
    break;
  }
  return os;
}

testing::AssertionResult AssertArrayEqual(const char*, const char*, const char*,
                                          const u32* expected, const u32* actual, size_t count)
{
  for (size_t i = 0; i < count; i++)
  {
    if (expected[i] != actual[i])
    {
      std::string msg = StringFromFormat("expected: 0x%08X (at index %zu)\nactual:   0x%08X",
                                         expected[i], i, actual[i]);
      return testing::AssertionFailure() << msg;
    }
  }
  return testing::AssertionSuccess();
}

class TextureDecoderTest : public testing::TestWithParam<CPU>
{
protected:
  CPUInfo real_cpu;

  void SetCPU(CPU cpu)
  {
    switch (cpu)
    {
    case CPU::Generic:
      cpu_info.bSSE2 = false;
      cpu_info.bSSSE3 = false;
      cpu_info.bSSE4_1 = false;
      cpu_info.bAVX2 = false;
      break;
    case CPU::SSE2:
      cpu_info.bSSE2 = real_cpu.bSSE2;
      cpu_info.bSSSE3 = false;
      cpu_info.bSSE4_1 = false;
      cpu_info.bAVX2 = false;
      break;
    case CPU::SSSE3:
      cpu_info.bSSE2 = real_cpu.bSSE2;
      cpu_info.bSSSE3 = real_cpu.bSSSE3;
      cpu_info.bSSE4_1 = false;
      cpu_info.bAVX2 = false;
      break;
    case CPU::SSE41:
      cpu_info.bSSE2 = real_cpu.bSSE2;
      cpu_info.bSSSE3 = real_cpu.bSSSE3;
      cpu_info.bSSE4_1 = real_cpu.bSSE4_1;
      cpu_info.bAVX2 = false;
      break;
    case CPU::AVX2:
      cpu_info.bSSE2 = real_cpu.bSSE2;
      cpu_info.bSSSE3 = real_cpu.bSSSE3;
      cpu_info.bSSE4_1 = real_cpu.bSSE4_1;
      cpu_info.bAVX2 = real_cpu.bAVX2;
      break;
    }
    TextureDecoder::Init();
  }

  void SetUp() override { SetCPU(GetParam()); }
  void TearDown() override { cpu_info = real_cpu; }
};

extern int gtest_TextureDecoderAllExtensionsTextureDecoderTest_dummy_;
INSTANTIATE_TEST_CASE_P(TextureDecoderAllExtensions, TextureDecoderTest,
                        testing::Values(CPU::Generic, CPU::SSE2, CPU::SSSE3, CPU::SSE41,
                                        CPU::AVX2));

// Fill a 1024x1024 texture with random data and compare
// the output of the different decoder implementations.
TEST_P(TextureDecoderTest, Random)
{
  const int iterations = 1;
  CPU param = GetParam();
  std::cerr << param << std::endl;
  unsigned width = 1024, height = width, num_pixels = width * height;
  size_t texture_size = num_pixels * 4;  // 4 is the maximum number of bytes per pixel.
  u8* src = (u8*)Common::AllocateAlignedMemory(texture_size, 32);
  u32* dst1 = (u32*)Common::AllocateAlignedMemory(texture_size, 32);
  u32* dst2 = (u32*)Common::AllocateAlignedMemory(texture_size, 32);

  std::srand(1);
  for (size_t i = 0; i < texture_size; i++)
    src[i] = rand() & 0xFF;

  u16 tlut[1 << 14];
  for (size_t i = 0; i < ArraySize(tlut); i++)
    tlut[i] = rand() & 0xFFFF;

  for (int format : {GX_TF_I4, GX_TF_I8, GX_TF_IA4, GX_TF_IA8, GX_TF_RGB565, GX_TF_RGB5A3,
                     GX_TF_RGBA8, GX_TF_CMPR})
  {
    SetCPU(CPU::Generic);
    TextureDecoder::Decode((u8*)dst1, src, width, height, format);
    SetCPU(param);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++)
      TextureDecoder::Decode((u8*)dst2, src, width, height, format);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    fprintf(stderr, "%6s: %4" PRId64 "\n", TextureDecoder::GetTextureFormatName(format),
            duration.count() >> 16);
    ASSERT_PRED_FORMAT3(AssertArrayEqual, dst1, dst2, num_pixels);
  }

  for (int format : {GX_TF_C4, GX_TF_C8, GX_TF_C14X2})
  {
    for (TlutFormat tlut_format : {GX_TL_IA8, GX_TL_RGB565, GX_TL_RGB5A3})
    {
      SetCPU(CPU::Generic);
      TextureDecoder::Decode((u8*)dst1, src, width, height, format, tlut, tlut_format);
      SetCPU(param);
      auto start = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < iterations; i++)
        TextureDecoder::Decode((u8*)dst2, src, width, height, format, tlut, tlut_format);
      auto end = std::chrono::high_resolution_clock::now();
      auto duration = end - start;
      fprintf(stderr, "%6s/%6s: %4" PRId64 "\n", TextureDecoder::GetTextureFormatName(format),
              TextureDecoder::GetTlutFormatName(tlut_format), duration.count() >> 16);
      ASSERT_PRED_FORMAT3(AssertArrayEqual, dst1, dst2, num_pixels);
    }
  }

  Common::FreeAlignedMemory(dst2);
  Common::FreeAlignedMemory(dst1);
  Common::FreeAlignedMemory(src);
}
