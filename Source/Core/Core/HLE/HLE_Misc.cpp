// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "Core/HLE/HLE_Misc.h"

#include "Common/Assert.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/GeckoCode.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace HLE_Misc
{
// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction(const Core::CPUThreadGuard&)
{
  auto& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();
  ppc_state.npc = LR(ppc_state);
}

void HBReload(const Core::CPUThreadGuard&)
{
  // There isn't much we can do. Just stop cleanly.
  auto& system = Core::System::GetInstance();
  system.GetCPU().Break();
  Host_Message(HostMessageID::WMUserStop);
}

void GeckoCodeHandlerICacheFlush(const Core::CPUThreadGuard& guard)
{
  auto& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();

  // Work around the codehandler not properly invalidating the icache, but
  // only the first few frames.
  // (Project M uses a conditional to only apply patches after something has
  // been read into memory, or such, so we do the first 5 frames.  More
  // robust alternative would be to actually detect memory writes, but that
  // would be even uglier.)
  u32 gch_gameid = PowerPC::MMU::HostRead_U32(guard, Gecko::INSTALLER_BASE_ADDRESS);
  if (gch_gameid - Gecko::MAGIC_GAMEID == 5)
  {
    return;
  }
  else if (gch_gameid - Gecko::MAGIC_GAMEID > 5)
  {
    gch_gameid = Gecko::MAGIC_GAMEID;
  }
  PowerPC::MMU::HostWrite_U32(guard, gch_gameid + 1, Gecko::INSTALLER_BASE_ADDRESS);

  ppc_state.iCache.Reset();
}

// Because Dolphin messes around with the CPU state instead of patching the game binary, we
// need a way to branch into the GCH from an arbitrary PC address. Branching is easy, returning
// back is the hard part. This HLE function acts as a trampoline that restores the original LR, SP,
// and PC before the magic, invisible BL instruction happened.
void GeckoReturnTrampoline(const Core::CPUThreadGuard& guard)
{
  auto& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();

  // Stack frame is built in GeckoCode.cpp, Gecko::RunCodeHandler.
  u32 SP = ppc_state.gpr[1];
  ppc_state.gpr[1] = PowerPC::MMU::HostRead_U32(guard, SP + 8);
  ppc_state.npc = PowerPC::MMU::HostRead_U32(guard, SP + 12);
  LR(ppc_state) = PowerPC::MMU::HostRead_U32(guard, SP + 16);
  ppc_state.cr.Set(PowerPC::MMU::HostRead_U32(guard, SP + 20));
  for (int i = 0; i < 14; ++i)
  {
    ppc_state.ps[i].SetBoth(PowerPC::MMU::HostRead_U64(guard, SP + 24 + 2 * i * sizeof(u64)),
                            PowerPC::MMU::HostRead_U64(guard, SP + 24 + (2 * i + 1) * sizeof(u64)));
  }
}

#if HAVE_FFMPEG
// Early GameCube games have a bug in their THP decoder.
void THPVideoDecode(const Core::CPUThreadGuard& guard)
{
  auto& system = Core::System::GetInstance();
  auto& mmu = system.GetMMU();
  auto& memory = system.GetMemory();
  auto& ppc_state = system.GetPPCState();

  u8* input = memory.GetPointer(mmu.GetTranslatedAddress(ppc_state.gpr[3]).value());

  av_log_set_level(AV_LOG_VERBOSE);
  AVPacket* packet = av_packet_alloc();
  ASSERT(packet);
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_THP);
  ASSERT(codec);
  AVCodecContext* context = avcodec_alloc_context3(codec);
  ASSERT(context);
  int result = avcodec_open2(context, codec, nullptr);
  ASSERT(result == 0);

  packet->data = input;
  packet->size = 1024 * 1024;
  result = avcodec_send_packet(context, packet);
  ASSERT_MSG(VIDEO, result == 0, "avcodec_send_packet: {}", result);
  AVFrame* frame = av_frame_alloc();
  ASSERT(frame);
  result = avcodec_receive_frame(context, frame);
  ASSERT_MSG(VIDEO, result == 0, "avcodec_receive_frame: {}", result);
  AVPixelFormat pix_fmt = (AVPixelFormat)frame->format;
  ASSERT(pix_fmt == AV_PIX_FMT_YUVJ420P);

  for (int plane = 0; plane < 3; ++plane)
  {
    int plane_width = plane ? frame->width >> 1 : frame->width;
    int plane_height = plane ? frame->height >> 1 : frame->height;
    ASSERT(frame->linesize[plane] == plane_width);
    auto ptr = mmu.GetTranslatedAddress(ppc_state.gpr[4 + plane]);
    ASSERT(ptr.has_value());
    const u8* src = frame->data[plane];
    u8* dst = reinterpret_cast<u8*>(memory.GetPointer(ptr.value()));

    // swizzle into I8 texture format
    for (int y = 0; y < plane_height; y += 4)
      for (int x = 0; x < plane_width; x += 8)
        for (int iy = 0; iy < 4; ++iy, dst += 8)
          memcpy(dst, src + (y + iy) * plane_width + x, 8);
  }

  avcodec_free_context(&context);
  av_frame_free(&frame);
  av_packet_free(&packet);

  ppc_state.gpr[3] = 0;  // success
  ppc_state.npc = LR(ppc_state);
}
#endif
}  // namespace HLE_Misc
