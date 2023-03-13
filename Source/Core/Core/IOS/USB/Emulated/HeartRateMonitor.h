// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/IOS/USB/Common.h"

namespace IOS::HLE::USB
{

// Heart Rate Monitor for EA Active 2 and NFL Training Camp
class HeartRateMonitor final : public Device
{
public:
  HeartRateMonitor(Kernel& ios);
  DeviceDescriptor GetDeviceDescriptor() const override;
  std::vector<ConfigDescriptor> GetConfigurations() const override;
  std::vector<InterfaceDescriptor> GetInterfaces(u8 config) const override;
  std::vector<EndpointDescriptor> GetEndpoints(u8 config, u8 interface, u8 alt) const override;
  bool Attach() override;
  bool AttachAndChangeInterface(u8 interface) override;
  int CancelTransfer(u8 endpoint) override;
  int ChangeInterface(u8 interface) override;
  int GetNumberOfAltSettings(u8 interface) override;
  int SetAltSetting(u8 alt_setting) override;
  int SubmitTransfer(std::unique_ptr<CtrlMessage> message) override;
  int SubmitTransfer(std::unique_ptr<BulkMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IntrMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IsoMessage> message) override;

private:
  Kernel& m_ios;
  u8 m_state;
};

}