// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceRVA.h"

#include <cctype>
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"


namespace ExpansionInterface
{

CEXIRVASerial::CEXIRVASerial() = default;
CEXIRVASerial::~CEXIRVASerial() = default;

void CEXIRVASerial::Reset()
{
}

void CEXIRVASerial::DoState(PointerWrap& p)
{
  p.Do(m_mode);
  p.Do(m_tx_status);
  p.Do(m_rx_status);
  p.Do(m_clock1);
  p.Do(m_clock2);
}

u32 CEXIRVASerial::Read(u32 address)
{
  switch (address)
  {
  case REG_MODE_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: reading status");
    return m_mode;
  case REG_TX:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: reading TX status");
    return m_tx_status;
  case REG_RX:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: RX");
    return 0x00;
  case REG_RX_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: reading RX status");
    return m_rx_status;
  case REG_CLOCK1:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: reading clock 1");
    return m_clock1;
  case REG_CLOCK2:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: reading clock 2");
    return m_clock2;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: reading unknown register {:08x}", address);
    return 0x00;
  }
}

void CEXIRVASerial::Write(u32 address, u32 value)
{
  switch (address)
  {
  case REG_MODE_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: setting mode to {:02x}", value);
    m_mode = value;
    break;
  case REG_TX:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: TX {:02x} {}", value, isprint((char)value) ? (char)value : '.');
    break;
  case REG_CLOCK1:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: setting clock 1 to {:02x}", value);
    m_clock1 = value;
    break;
  case REG_CLOCK2:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: setting clock 2 to {:02x}", value);
    m_clock2 = value;
    break;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-UART: setting unknown register {:08x} to {:08x}", address, value);
    break;
  }
}


CEXIRVAJVS::CEXIRVAJVS() = default;
CEXIRVAJVS::~CEXIRVAJVS() = default;

void CEXIRVAJVS::Reset()
{
}

void CEXIRVAJVS::DoState(PointerWrap& p)
{
  p.Do(m_mode);
  p.Do(m_tx_status);
  p.Do(m_rx_status);
  p.Do(m_tx_len);
  p.Do(m_rx_len);
  p.Do(m_unk18);
}

u32 CEXIRVAJVS::Read(u32 address)
{
  switch (address)
  {
  case REG_MODE_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: reading status");
    return m_mode;
  case REG_TX:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: reading TX status");
    return m_tx_status;
  case REG_RX:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: RX <- 0");
    return 0x00;
  case REG_RX_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: reading RX status");
    return m_rx_status;
  case REG_TX_LEN:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: reading TX length");
    return m_tx_len;
  case REG_RX_LEN:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: reading RX length");
    return m_rx_len;
  case REG_UNK18:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: reading unknown register 0x18");
    return m_unk18;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: reading unknown register {:08x}", address);
    return 0x00;
  }
}

void CEXIRVAJVS::Write(u32 address, u32 value)
{
  switch (address)
  {
  case REG_MODE_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: setting mode to {:02x}", value);
    m_mode = value;
    break;
  case REG_TX:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: TX -> {:02x} : {}",
      value, isprint((char)value) ? (char)value : '.');
    break;
  case REG_TX_LEN:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: setting TX length to {:02x}", value);
    m_tx_len = value;
    break;
  case REG_RX_LEN:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: setting RX length to {:02x}", value);
    m_rx_len = value;
    break;
  case REG_UNK18:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: setting unknown register 0x18 to {:08x}", value);
    m_unk18 = value;
    break;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-JVS: setting unknown register {:08x} to {:08x}", address, value);
    break;
  }
}


CEXIRVA::CEXIRVA(Core::System& system) : IEXIDevice(system), m_map(), m_jvs(), m_uart0(), m_uart1()
{
  m_board_status = 0;
  m_map.Map(DEVICE_BASE, DEVICE_SIZE,
    [&](u32 address) { return this->ReadDevice(address); },
    [&](u32 address, u32 value) { return this->WriteDevice(address, value); }
  );
  m_map.Map(BOARD_BASE, BOARD_SIZE,
    [&](u32 address) { return this->ReadBoard(address); },
    [&](u32 address, u32 value) { return this->WriteBoard(address, value); }
  );
  m_map.Map(SWITCH_BASE, SWITCH_SIZE,
    [&](u32 address) { return this->ReadSwitch(address); },
    [&](u32 address, u32 value) { return this->WriteSwitch(address, value); }
  );
  m_map.Map(JVS_BASE, JVS_SIZE,
    [&](u32 address) { return m_jvs.Read(address); },
    [&](u32 address, u32 value) { return m_jvs.Write(address, value); }
  );
  m_map.Map(UART0_BASE, UART0_SIZE,
    [&](u32 address) { return m_uart0.Read(address); },
    [&](u32 address, u32 value) { return m_uart0.Write(address, value); }
  );
  m_map.Map(UART1_BASE, UART1_SIZE,
    [&](u32 address) { return m_uart1.Read(address); },
    [&](u32 address, u32 value) { return m_uart1.Write(address, value); }
  );
}

CEXIRVA::~CEXIRVA() = default;

void CEXIRVA::SetCS(int cs)
{
  if (cs)
  {
    m_map.Reset();
    m_jvs.Reset();
    m_uart0.Reset();
    m_uart1.Reset();
  }
}

bool CEXIRVA::IsPresent() const
{
  return true;
}

void CEXIRVA::DoState(PointerWrap& p)
{
  m_map.DoState(p);
  p.Do(m_board_status);
  m_jvs.DoState(p);
  m_uart0.DoState(p);
  m_uart1.DoState(p);
}

void CEXIRVA::ImmWrite(u32 data, u32 size)
{
  m_map.Write(data, size);
}

u32 CEXIRVA::ImmRead(u32 size)
{
  return m_map.Read(size);
}

u32 CEXIRVA::ReadDevice(u32 address)
{
  switch (address)
  {
  case DEVICE_REG_ID:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-DEV: reading ID");
    return 0x13371337; // TODO
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-DEV: reading unknown register {:08x}", address);
    return 0x00;
  }
}

void CEXIRVA::WriteDevice(u32 address, u32 value)
{
  switch (address)
  {
  case DEVICE_REG_ID:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-DEV: setting ID to {:08x}", value);
    break;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-DEV: setting unknown register {:08x} to {:08x}", address, value);
    break;
  }
}

u32 CEXIRVA::ReadBoard(u32 address)
{
  switch (address)
  {
  case BOARD_REG_WATCHDOG:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-BOARD: reading watchdog");
    return 0x00;
  case BOARD_REG_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-BOARD: reading status");
    return m_board_status;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-BOARD: reading unknown register {:08x}", address);
    return 0x00;
  }
}

void CEXIRVA::WriteBoard(u32 address, u32 value)
{
  switch (address)
  {
  case BOARD_REG_WATCHDOG:
    DEBUG_LOG_FMT(EXPANSIONINTERFACE, "RVA-BOARD: poking watchdog");
    break;
  case BOARD_REG_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-BOARD: setting status to {:08x}", value);
    m_board_status = value;
    break;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-BOARD: setting unknown register {:08x} to {:08x}", address, value);
    break;
  }
}

u32 CEXIRVA::ReadSwitch(u32 address)
{
  switch (address)
  {
  case SWITCH_REG_STATUS:
    DEBUG_LOG_FMT(EXPANSIONINTERFACE, "RVA-SWITCH: reading status");
    return 0xFF;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-SWITCH: reading unknown register {:08x}", address);
    return 0x00;
  }
}

void CEXIRVA::WriteSwitch(u32 address, u32 value)
{
  switch (address)
  {
  case SWITCH_REG_STATUS:
    INFO_LOG_FMT(EXPANSIONINTERFACE, "RVA-SWITCH: setting status to {:08x}", value);
    break;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "RVA-SWITCH: setting unknown register {:08x} to {:08x}", value, address);
    break;
  }
}

}  // namespace ExpansionInterface
