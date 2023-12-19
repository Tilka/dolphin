// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_AddressMap.h"

#include "Common/ChunkFile.h"

namespace ExpansionInterface
{

CEXIAddressMap::CEXIAddressMap() = default;
CEXIAddressMap::~CEXIAddressMap() = default;

void CEXIAddressMap::Reset()
{
  m_addr = 0;
  m_pos = 0;
}

void CEXIAddressMap::DoState(PointerWrap& p)
{
  p.Do(m_addr);
  p.Do(m_pos);
  //p.Do(m_map);
}

void CEXIAddressMap::Map(Address start, Address size, ReadCallback on_read, WriteCallback on_write)
{
  Range range = {
    .size = size,
    .read = on_read,
    .write = on_write,
  };
  m_map[start] = std::move(range);
}

void CEXIAddressMap::Unmap(Address addr, Address size)
{
  do {
    // Find matching entry
    auto entry = m_map.lower_bound(addr);
    if (entry == m_map.end())
    {
      break;
    }

    Address start = entry->first;
    auto range = entry->second;
    if (addr >= start + range.size)
    {
      break;
    }

    // Adjust entry
    m_map.erase(start);
    bool reinsert = false;
    if (addr > start)
    {
      range.size -= (addr - start);
      reinsert = true;
    }
    if (size < range.size)
    {
      range.size = size;
      reinsert = true;
      size = 0;
    } else {
      size -= range.size;
      addr += range.size;
    }

    // Reinsert, perhaps
    if (reinsert)
    {
      m_map[start] = std::move(range);
    }
  } while (size);
}


u32 CEXIAddressMap::Read(u32 size)
{
  if (m_pos < sizeof(u32))
  {
    return 0xffffffff;
  }
  else if (auto entry = At(m_addr))
  {
    auto offset = entry->first;
    auto& range = entry->second;
    u32 value = range.read(offset);
    u32 data = 0;
    while (size--)
    {
      u8 b = value >> (size * 8);
      data |= b << ((3 - size) * 8);
    }
    return data;
  }
  else
  {
    INFO_LOG_FMT(EXPANSIONINTERFACE, "MAP: read gap {:08x}", m_addr);
    return 0xffffffff;
  }
}

void CEXIAddressMap::Write(u32 data, u32 size)
{
  // Fill up address bytes
  if (m_pos < sizeof(Address))
  {
    m_addr |= Address(data) << ((sizeof(Address) - sizeof(u32) - m_pos) * 8);
    m_pos += sizeof(u32);
  }
  else if (auto entry = At(m_addr))
  {
    auto offset = entry->first;
    auto& range = entry->second;
    u32 value = 0;
    while (size--)
    {
      u8 b = data >> (size * 8);
      value |= b << ((3 - size) * 8);
    }
    range.write(offset, value);
  }
  else
  {
    INFO_LOG_FMT(EXPANSIONINTERFACE, "MAP: write {:08x} to gap at {:08x}", data, m_addr);
  }
}

std::optional<std::pair<CEXIAddressMap::Address, CEXIAddressMap::Range>> CEXIAddressMap::At(Address raw_addr)
{
  Address addr = (raw_addr & ~0x80000000) >> 6;

  Address start;
  bool found = false;

  auto entry_it = m_map.upper_bound(addr);
  if (entry_it == m_map.end())
  {
    if (!m_map.empty())
    {
      start = m_map.rbegin()->first;
      found = true;
    }
  }
  else if (entry_it != m_map.begin())
  {
    start = std::prev(entry_it)->first;
    found = true;
  }

  if (found)
  {
    auto& range = m_map[start];
    if (start <= addr && start + range.size > addr)
    {
      return std::make_pair(addr - start, range);
    }
  }

  return {};
}

}  // namespace ExpansionInterface
