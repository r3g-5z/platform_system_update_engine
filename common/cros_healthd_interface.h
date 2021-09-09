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

#ifndef UPDATE_ENGINE_COMMON_CROS_HEALTHD_INTERFACE_H_
#define UPDATE_ENGINE_COMMON_CROS_HEALTHD_INTERFACE_H_

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <base/callback.h>
#include <base/optional.h>

namespace chromeos_update_engine {

enum class TelemetryCategoryEnum {
  kBattery = 0,
  kNonRemovableBlockDevices = 1,
  kCpu = 2,
  kTimezone = 3,
  kMemory = 4,
  kBacklight = 5,
  kFan = 6,
  kStatefulPartition = 7,
  kBluetooth = 8,
  kSystem = 9,
  kSystem2 = 10,
  kNetwork = 11,
  kAudio = 12,
  kBootPerformance = 13,
  kBus = 14,
};

// This structure represents the telemetry information collected from
// `cros_healthd`.
typedef struct TelemtryInfo {
  // SystemV2 information.
  typedef struct SystemV2Info {
    // DMI to identify hardware.
    typedef struct DmiInfo {
      std::string board_vendor;
      std::string board_name;
      std::string board_version;
      std::string bios_version;
    } DmiInfo;
    DmiInfo dmi_info;

    // OS information.
    typedef struct OsInfo {
      enum class BootMode : int32_t {
        kUnknown = 0,
        kCrosSecure = 1,
        kCrosEfi = 2,
        kCrosLegacy = 3,
      };
      BootMode boot_mode;
    } OsInfo;
    OsInfo os_info;
  } SystemV2Info;
  SystemV2Info system_v2_info;

  // Memory information.
  typedef struct MemoryInfo {
    uint32_t total_memory_kib;
  } MemoryInfo;
  MemoryInfo memory_info;

  // Non-removable block device information.
  typedef struct NonRemovableBlockDeviceInfo {
    uint64_t size;
  } NonRemovableBlockDeviceInfo;
  std::vector<NonRemovableBlockDeviceInfo> block_device_info;

  // CPU information.
  typedef struct CpuInfo {
    // Physical CPU information.
    typedef struct PhysicalCpuInfo {
      std::string model_name;
    } PhysicalCpuInfo;
    std::vector<PhysicalCpuInfo> physical_cpus;
  } CpuInfo;
  CpuInfo cpu_info;
} TelemetryInfo;

// The abstract cros_healthd interface defines the interaction with the
// platform's cros_healthd.
class CrosHealthdInterface {
 public:
  CrosHealthdInterface(const CrosHealthdInterface&) = delete;
  CrosHealthdInterface& operator=(const CrosHealthdInterface&) = delete;

  virtual ~CrosHealthdInterface() = default;

  virtual bool Init() = 0;

  virtual TelemetryInfo* const GetTelemetryInfo() = 0;

  // Returns telemetry information for the desired categories in callback.
  // Limited to `TelemetryInfo` as the avaiable telemetry is vast.
  using ProbeTelemetryInfoCallback =
      base::OnceCallback<void(const TelemetryInfo&)>;
  virtual void ProbeTelemetryInfo(
      const std::unordered_set<TelemetryCategoryEnum>& categories,
      ProbeTelemetryInfoCallback once_callback) = 0;

 protected:
  CrosHealthdInterface() = default;
};

// This factory function creates a new CrosHealthdInterface instance for the
// current platform.
std::unique_ptr<CrosHealthdInterface> CreateCrosHealthd();

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_CROS_HEALTHD_INTERFACE_H_
