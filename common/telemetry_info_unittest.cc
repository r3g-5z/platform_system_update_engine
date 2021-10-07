//
// Copyright (C) 2021 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/common/telemetry_info.h"

#include <gtest/gtest.h>

namespace chromeos_update_engine {

class TelemetryInfoTest : public ::testing::Test {
 protected:
  TelemetryInfo telemetry_info;
};

TEST_F(TelemetryInfoTest, GetWirelessDrivers) {
  telemetry_info.bus_devices = {
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .driver = "fake-driver-1",
              },
      },
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .driver = "fake-driver-2",
              },
      },
      // Should ignore USB bus type.
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
          .bus_type_info = TelemetryInfo::BusDevice::UsbBusInfo{},
      },
      // Should ignore non wireless controller.
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kDisplayController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .driver = "should-not-be-included",
              },
      },
  };
  EXPECT_EQ("fake-driver-1 fake-driver-2", telemetry_info.GetWirelessDrivers());
}

TEST_F(TelemetryInfoTest, GetWirelessIds) {
  telemetry_info.bus_devices = {
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .vendor_id = 1,
                  .device_id = 2,
              },
      },
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .vendor_id = 3,
                  .device_id = 4,
              },
      },
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
          .bus_type_info =
              TelemetryInfo::BusDevice::UsbBusInfo{
                  .vendor_id = 5,
                  .product_id = 6,
              },
      },
      // Should ignore non wireless controller.
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kDisplayController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .vendor_id = 7,
                  .device_id = 8,
              },
      },
  };
  EXPECT_EQ("0100:0200 0300:0400 0500:0600", telemetry_info.GetWirelessIds());
}

TEST_F(TelemetryInfoTest, GetGpuIds) {
  telemetry_info.bus_devices = {
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kDisplayController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .vendor_id = 1,
                  .device_id = 2,
              },
      },
      // Should ignore non display controller.
      {
          .device_class =
              TelemetryInfo::BusDevice::BusDeviceClass::kWirelessController,
          .bus_type_info =
              TelemetryInfo::BusDevice::PciBusInfo{
                  .vendor_id = 3,
                  .device_id = 4,
              },
      },
  };
  EXPECT_EQ("0100:0200", telemetry_info.GetGpuIds());
}

}  // namespace chromeos_update_engine
