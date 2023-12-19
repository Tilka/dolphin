// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <utility>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace ExpansionInterface
{

class CEXIAddressMap
{
public:
  using Address = u32;
  using ReadCallback = std::function<u32(Address)>;
  using WriteCallback = std::function<void(Address, u32)>;

  explicit CEXIAddressMap();
  ~CEXIAddressMap();

  void Reset();
  void DoState(PointerWrap& p);
  void TransferByte(u8& data);
  u32  Read(u32 size);
  void Write(u32 data, u32 size);

  void Map(Address addr, Address size, ReadCallback, WriteCallback);
  void Unmap(Address addr, Address size);

private:
  struct Range
  {
    Address       size;
    ReadCallback  read;
    WriteCallback write;
  };

  Address m_addr;
  size_t  m_pos;
  std::map<Address, Range> m_map;

  std::optional<std::pair<Address, Range>> At(Address raw_addr);
};

}  // namespace ExpansionInterface
