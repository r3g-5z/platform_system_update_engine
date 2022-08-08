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

#ifndef UPDATE_ENGINE_COMMON_CROS_HEALTHD_H_
#define UPDATE_ENGINE_COMMON_CROS_HEALTHD_H_

#include "update_engine/common/cros_healthd_interface.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <dbus/object_proxy.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <mojo/core/embedder/embedder.h>
#include <mojo/core/embedder/scoped_ipc_support.h>

#include "diagnostics/cros_healthd_mojo_adapter/cros_healthd_mojo_adapter.h"
#include "diagnostics/mojom/public/cros_healthd_probe.mojom.h"

namespace chromeos_update_engine {

class CrosHealthd : public CrosHealthdInterface {
 public:
  CrosHealthd()
      : telemetry_info_(std::make_unique<TelemetryInfo>()),
        weak_ptr_factory_(this) {}
  CrosHealthd(const CrosHealthd&) = delete;
  CrosHealthd& operator=(const CrosHealthd&) = delete;

  ~CrosHealthd() = default;

  void Init();

  // CrosHealthdInterface overrides.
  void BootstrapMojo(BootstrapMojoCallback callback) override;
  TelemetryInfo* const GetTelemetryInfo() override;
  void ProbeTelemetryInfo(
      const std::unordered_set<TelemetryCategoryEnum>& categories,
      ProbeTelemetryInfoCallback once_callback) override;

 private:
  FRIEND_TEST(CrosHealthdTest, ParseSystemResultCheck);
  FRIEND_TEST(CrosHealthdTest, ParseMemoryResultCheck);
  FRIEND_TEST(CrosHealthdTest, ParseNonRemovableBlockDeviceResultCheck);
  FRIEND_TEST(CrosHealthdTest, ParseCpuResultCheck);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckMissingBusResult);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckMissingBusInfo);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckPciBusDefault);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckPciBus);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckUsbBusDefault);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckUsbBus);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckThunderboltBusDefault);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckThunderboltBus);
  FRIEND_TEST(CrosHealthdTest, ParseBusResultCheckAllBus);

  // Get `cros_healthd` DBus object proxy.
  dbus::ObjectProxy* GetCrosHealthdObjectProxy();

  void FinalizeBootstrap(BootstrapMojoCallback callback,
                         bool service_available);

  void OnProbeTelemetryInfo(
      ProbeTelemetryInfoCallback once_callback,
      chromeos::cros_healthd::mojom::TelemetryInfoPtr result);

  // Parsing helpers for `OnProbTelemetryInfo()` .
  bool ParseSystemResult(
      chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
      TelemetryInfo* telemetry_info);
  bool ParseMemoryResult(
      chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
      TelemetryInfo* telemetry_info);
  bool ParseNonRemovableBlockDeviceResult(
      chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
      TelemetryInfo* telemetry_info);
  bool ParseCpuResult(chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
                      TelemetryInfo* telemetry_info);
  bool ParseBusResult(chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
                      TelemetryInfo* telemetry_info);

  std::unique_ptr<TelemetryInfo> telemetry_info_;

  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  mojo::Remote<chromeos::cros_healthd::mojom::CrosHealthdServiceFactory>
      cros_healthd_service_factory_;

  mojo::Remote<chromeos::cros_healthd::mojom::CrosHealthdProbeService>
      cros_healthd_probe_service_;

  base::WeakPtrFactory<CrosHealthd> weak_ptr_factory_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_COMMON_CROS_HEALTHD_H_
