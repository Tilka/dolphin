// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/EXI/EXI_AddressMap.h"
#include "Core/HW/EXI/EXI_Device.h"

class PointerWrap;

namespace ExpansionInterface
{

class CEXIRVASerial
{
public:
  explicit CEXIRVASerial();
  ~CEXIRVASerial();

  void Reset();
  void DoState(PointerWrap& p);
  u32  Read(u32 address);
  void Write(u32 address, u32 value);

  enum
  {
    REG_MODE_STATUS = 0x00,
    REG_TX          = 0x04,
    REG_RX          = 0x08,
    REG_RX_STATUS   = 0x0C,
    REG_CLOCK1      = 0x10,
    REG_CLOCK2      = 0x14,
    REGSIZE         = 0x18,
  };
private:
  u32 m_mode;
  u32 m_tx_status;
  u32 m_rx_status;
  u32 m_clock1;
  u32 m_clock2;
};


class CEXIRVAJVS
{
public:
  explicit CEXIRVAJVS();
  ~CEXIRVAJVS();

  void Reset();
  void DoState(PointerWrap& p);
  u32  Read(u32 address);
  void Write(u32 address, u32 value);

  enum
  {
    REG_MODE_STATUS = 0x00,
    REG_TX          = 0x04,
    REG_RX          = 0x08,
    REG_RX_STATUS   = 0x0C,
    REG_TX_LEN      = 0x10,
    REG_RX_LEN      = 0x14,
    REG_UNK18       = 0x18,
    REGSIZE         = 0x1C,
  };

private:
  u32 m_mode;
  u32 m_tx_status;
  u32 m_rx_status;
  u32 m_tx_len;
  u32 m_rx_len;
  u32 m_unk18;
};

class CEXIRVA : public IEXIDevice
{
public:
  explicit CEXIRVA(Core::System& system);
  ~CEXIRVA() override;

  bool IsPresent() const override;
  void SetCS(int cs) override;
  void DoState(PointerWrap& p) override;

private:
  // TODO: confirm these range sizes
  enum
  {
    DEVICE_REG_ID  = 0x00,
    DEVICE_REGSIZE = 0x04,
  };

  enum
  {
    BOARD_REG_WATCHDOG = 0x00,
    BOARD_REG_STATUS   = 0x1C,
    BOARD_REGSIZE      = 0x20,
  };

  enum
  {
    SWITCH_REG_STATUS = 0x00,
    SWITCH_REGSIZE    = 0x04,
  };

  enum
  {
    // EXI device
    DEVICE_BASE = 0x000000,
    DEVICE_SIZE = DEVICE_REGSIZE,
    // Board
    BOARD_BASE  = 0x800000,
    BOARD_SIZE  = BOARD_REGSIZE,
    // Switch
    SWITCH_BASE = 0x900000,
    SWITCH_SIZE = SWITCH_REGSIZE,
    // JVS
    JVS_BASE    = 0xB00000,
    JVS_SIZE    = CEXIRVAJVS::REGSIZE,
    // Serial 1
    UART0_BASE  = 0xC00000,
    UART0_SIZE  = CEXIRVASerial::REGSIZE,
    // Serial 2
    UART1_BASE  = 0xD00000,
    UART1_SIZE  = CEXIRVASerial::REGSIZE,
  };


  CEXIAddressMap m_map;
  u32            m_board_status;
  CEXIRVAJVS     m_jvs;
  CEXIRVASerial  m_uart0;
  CEXIRVASerial  m_uart1;

  void ImmWrite(u32 data, u32 size) override;
  u32  ImmRead(u32 size) override;

  u32  ReadDevice(u32 address);
  void WriteDevice(u32 address, u32 value);

  u32  ReadBoard(u32 address);
  void WriteBoard(u32 address, u32 value);

  u32  ReadSwitch(u32 address);
  void WriteSwitch(u32 address, u32 value);
};

}  // namespace ExpansionInterface
