// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/HeartRateMonitor.h"

namespace IOS::HLE::USB
{

HeartRateMonitor::HeartRateMonitor(Kernel& ios) : m_ios(ios), m_state(0)
{
}

DeviceDescriptor HeartRateMonitor::GetDeviceDescriptor() const
{
  return DeviceDescriptor{
    .bLength = 18,
    .bDescriptorType = 1,
    .bcdUSB = 0x110,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 8,
    .idVendor = 0x21A4,
    .idProduct = 0xAC40,
    .bcdDevice = 0x300,
    .iManufacturer = 1, // "Licensed by Nintendo of America"
    .iProduct = 2, // "EA SPORTS Active(tm) (c) 2010 Electronic Arts Inc."
    .iSerialNumber = 3, // e.g. "00052978"
    .bNumConfigurations = 1,
  };
}

std::vector<ConfigDescriptor> HeartRateMonitor::GetConfigurations() const
{
  return {
    ConfigDescriptor{
        .bLength = 9,
        .bDescriptorType = 2,
        .wTotalLength = 0x0022,
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = 0,
        .bmAttributes = 0x80,
        .MaxPower = 50, // 100 mA
    }
  };
}

std::vector<InterfaceDescriptor> HeartRateMonitor::GetInterfaces(u8 config) const
{
  return {
    InterfaceDescriptor{
        .bLength = 9,
        .bDescriptorType = 4,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 1,
        .bInterfaceClass = 3, // HID
        .bInterfaceSubClass = 0,
        .iInterface = 0,
    }
  };
}

std::vector<EndpointDescriptor> HeartRateMonitor::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  return {
    EndpointDescriptor{
        .bLength = 7,
        .bDescriptorType = 5,
        .bEndpointAddress = 0x81,
        .bmAttributes = 3,
        .wMaxPacketSize = 0x0010,
        .bInterval = 16,
    }
  };
}

bool HeartRateMonitor::Attach()
{
  return true;
}

bool HeartRateMonitor::AttachAndChangeInterface(u8 interface)
{
  return true;
}

int HeartRateMonitor::CancelTransfer(u8 endpoint)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME HRM: CancelTransfer");
  return 0;
}

int HeartRateMonitor::ChangeInterface(u8 interface)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME HRM: ChangeInterface");
  return 0;
}

int HeartRateMonitor::GetNumberOfAltSettings(u8 interface)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME HRM: GetNumberOfAltSettings");
  return 0;
}

int HeartRateMonitor::SetAltSetting(u8 alt_setting)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME HRM: SetAltSetting");
  return 0;
}

int HeartRateMonitor::SubmitTransfer(std::unique_ptr<CtrlMessage> message)
{
  constexpr u8 HID_SET_REPORT = 9;
  constexpr u16 REPORT_0_OUTPUT = 0x0200;
  if (message->request_type == 0x21 && message->request == HID_SET_REPORT &&
      message->value == REPORT_0_OUTPUT && message->length == 2)
  {
    std::unique_ptr<u8[]> cmd = message->MakeBuffer(message->length);
    u8 device = (cmd[0] & 0x60) == 0x60 ? 0 : 1;
    u8 hrm_filter_mode = cmd[1] & 0x1F;
    cmd[0] &= ~0x60;
    if (cmd[0] == 0x14 && cmd[1] == 0x13)
    {
      ERROR_LOG_FMT(IOS_USB, "HRM: reset?");
      m_state = 0; // just a guess
    }
    if (cmd[0] == 0x14 && cmd[1] == 0x93)
    {
      ERROR_LOG_FMT(IOS_USB, "HRM: pairing mode");
    }
    if (cmd[0] == 0x95 && cmd[1] == 0x13)
    {
      ERROR_LOG_FMT(IOS_USB, "HRM: disconnect");
    }
  }
  message->ScheduleTransferCompletion(message->length, 0);
  return 0;
}

int HeartRateMonitor::SubmitTransfer(std::unique_ptr<BulkMessage> message)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME HRM: BulkMessage");
  return 0;
}

int HeartRateMonitor::SubmitTransfer(std::unique_ptr<IntrMessage> message)
{
  //WARN_LOG_FMT(IOS_USB, "HRM: IntrMessage({})", m_state);
  // Differences between Wii and PS3:
  // - PS3 uses 11+11+9 byte packets instead of 16+15 (why?)
  // - PS3 ships with a right-arm accelerometer, Wii uses Wii remote instead
  u8 response[16];
  if (m_state == 0)
  {
    std::fill(std::begin(response), std::end(response), 0x80);
    message->FillBuffer(response, 16);
    message->ScheduleTransferCompletion(16, 1000);
    m_state = 1;
  }
  else
  {
#if 0
    std::fill(std::begin(response), std::end(response), 0x80);
    response[0x0B] = 0x00;
    response[0x0C] = 0x00;
    response[0x0D] = 0x50;
    response[0x0E] = 0x12;
#else
    std::fill(std::begin(response), std::end(response), 0x80);
    response[0x0B] = 0x7B; // heart rate
    response[0x0C] = 0x0C; // heart rate confidence?
    response[0x0D] = 0x00 | 0x20; // left arm: 0x10 = battery < 20%, 0x20 = connected; right arm (PS3): 0x04 = connected
    response[0x0E] = 0x00 | 0x20; // right leg: 0x10 = battery < 20%, 0x20 = connected
#endif
    message->FillBuffer(response, 15);
    message->ScheduleTransferCompletion(15, 1000);
    m_state = 0;
  }
  return 0;
}

int HeartRateMonitor::SubmitTransfer(std::unique_ptr<IsoMessage> message)
{
  ERROR_LOG_FMT(IOS_USB, "FIXME: HRM IsoMessage");
  return 0;
}

} // namespace IOS::HLE::USB