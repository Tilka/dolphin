// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/FifologCommand.h"

#include <OptionParser.h>
#include <fmt/ostream.h>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/FifoPlayer/FifoDataFile.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"
#include "UICommon/UICommon.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/XFStructs.h"

namespace DolphinTool
{

class ToolDecoder : OpcodeDecoder::Callback
{
public:
  ToolDecoder(Core::System& system, const CPState& cpmem, bool raw)
      : m_system(system), m_cpmem(cpmem), m_raw(raw)
  {
  }

  // Called on any XF command.
  void OnXF(u16 address, u8 count, const u8* data) override
  {
    fmt::print("XF direct: address {:04X} count {:02X} ", address, count);
    for (u8 i = 0; i < count; ++i)
    {
      u32 value = Common::swap32(data + sizeof(u32) * i);
      auto info = GetXFRegInfo(address + i, value);
      for (char& c : info.second)
        if (c == '\n')
          c = '|';
      fmt::print(" {:08X} ({}: {})", value, info.first, info.second);
    }
    fmt::println("");
  }
  // Called on any CP command.
  // Subclasses should update the CP state with GetCPState().LoadCPReg(command, value) so that
  // primitive commands decode properly.
  void OnCP(u8 command, u32 value) override
  {
    auto info = GetCPRegInfo(command, value);
    for (char& c : info.second)
      if (c == '\n')
        c = '|';
    fmt::println("CP: command {:02X} value {:08X} ({}: {})", command, value, info.first,
                 info.second);
    GetCPState().LoadCPReg(command, value);
  }
  // Called on any BP command.
  void OnBP(u8 command, u32 value) override
  {
    auto info = GetBPRegInfo(command, value);
    for (char& c : info.second)
      if (c == '\n')
        c = '|';
    fmt::println("BP: command {:02X} value {:06X} ({}: {})", command, value, info.first,
                 info.second);
  }
  // Called on any indexed XF load command.
  void OnIndexedLoad(CPArray array, u32 index, u16 address, u8 size) override
  {
    fmt::print("XF indirect: array '{}' index {:08X} address {:04X} size {:02X} ", array, index,
               address, size);
    u32 base = m_cpmem.array_bases[array];
    u32 stride = m_cpmem.array_strides[array];
    for (u8 i = 0; i < size; ++i)
    {
      // FIXME: this is probably wrong
      u32 value = m_system.GetMemory().Read_U32(base + index * stride);
      fmt::print("{:08X} ", value);
    }
    fmt::println("");
  }
  // Called on any primitive command.
  void OnPrimitiveCommand(OpcodeDecoder::Primitive primitive, u8 vat, u32 vertex_size,
                          u16 num_vertices, const u8* vertex_data) override
  {
    fmt::println("{} VAT {} vertex size {} count {}", primitive, vat, vertex_size, num_vertices);
  }
  // Called on a display list.
  void OnDisplayList(u32 address, u32 size) override
  {
    fmt::println("Display List: address {:08X} size {:08X}", address, size);
  }
  // Called on any NOP commands (which are all merged into a single call).
  void OnNop(u32 count) override { fmt::println("nop {}", count); }
  // Called on an unknown opcode, or an opcode that is known but not implemented.
  // data[0] is opcode.
  void OnUnknown(u8 opcode, const u8* data) override
  {
    auto op = static_cast<OpcodeDecoder::Opcode>(opcode);
    if (op == OpcodeDecoder::Opcode::GX_CMD_UNKNOWN_METRICS)
      fmt::println("Unknown Metrics");
    else if (op == OpcodeDecoder::Opcode::GX_CMD_INVL_VC)
      fmt::println("Invalidate Vertex Cache");
    else
      fmt::println("UNKNOWN");
  }

  // Called on ANY command.  The first byte of data is the opcode.  Size will be at least 1.
  // This function is called after one of the above functions is called.
  void OnCommand(const u8* data, u32 size) override
  {
    if (m_raw)
    {
      fmt::print("(raw:");
      for (u32 pos = 0; pos < size; ++pos)
        fmt::print(" {:02X}", data[pos]);
      fmt::println(")");
    }
  }

  // Get the current CP state.  Needed for vertex decoding; will also be mutated for CP commands.
  CPState& GetCPState() override { return m_cpmem; }

  u32 GetVertexSize(u8 vat) override
  {
    return VertexLoaderBase::GetVertexSize(GetCPState().vtx_desc, GetCPState().vtx_attr[vat]);
  }

private:
  Core::System& m_system;
  CPState m_cpmem;
  const bool m_raw;
};

int Fifolog(const std::vector<std::string>& args)
{
  optparse::OptionParser parser;

  parser.usage("usage: fifolog [options]...");

  parser.add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to fifolog FILE.")
      .metavar("FILE");
  parser.add_option("--frame")
      .type("int")
      .action("store")
      .help("Frame number, starting at 0.")
      .metavar("FRAME")
      .set_default(0);
  parser.add_option("--raw")
      .action("store_true")
      .help("If set, also output raw fifo bytes for each command");
  parser.add_option("-u", "--user")
      .type("string")
      .action("store")
      .help("User folder path, required for temporary processing files. "
            "Will be automatically created if this option is not set.")
      .set_default("");

  const optparse::Values& options = parser.parse_args(args);
  if (!options.is_set("input"))
  {
    fmt::println(std::cerr, "Error: No input image set");
    return EXIT_FAILURE;
  }

  UICommon::SetUserDirectory(options["user"]);
  UICommon::Init();

  // FifoDataFile::Load() needs to be called twice, don't ask...
  FifoDataFile::Load(options["input"], true);
  auto& system = Core::System::GetInstance();
  system.Initialize();
  system.GetMemory().Init();

  FifoPlayer player(system);
  if (!player.Open(options["input"]))
  {
    fmt::println(std::cerr, "Error: Failed to open fifolog");
    return EXIT_FAILURE;
  }

  u32 frame_number = 0;
  if (options.is_set("frame"))
  {
    frame_number = (u32)(int)options.get("frame");
    if (frame_number > player.GetFrameRangeEnd())
    {
      fmt::println(std::cerr, "Error: Frame number out of range");
      return EXIT_FAILURE;
    }
  }

  int object = 0;
  int efb_copy = 0;
  const bool raw = options.is_set("raw");
  const FifoFrameInfo& raw_frame = player.GetFile()->GetFrame(frame_number);
  const AnalyzedFrameInfo& analyzed_frame = player.GetAnalyzedFrameInfo(frame_number);
  const u8* const data = raw_frame.fifoData.data();

  for (const FramePart& part : analyzed_frame.parts)
  {
    switch (part.m_type)
    {
    case FramePartType::Commands:
      fmt::println("Object {}", object++);
      break;
    case FramePartType::PrimitiveData:
      // just print the draws
      break;
    case FramePartType::EFBCopy:
      fmt::println("EFB Copy {}", efb_copy++);
      break;
    }

    ToolDecoder callback(system, part.m_cpmem, raw);
    OpcodeDecoder::Run(&data[part.m_start], part.m_end - part.m_start, callback);

    fmt::println("");
  }

  return EXIT_SUCCESS;
}
}  // namespace DolphinTool
