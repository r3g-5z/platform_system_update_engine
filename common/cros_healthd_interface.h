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
#include <unordered_set>

#include <base/callback.h>

#include "update_engine/common/telemetry_info.h"

namespace chromeos_update_engine {

// The abstract cros_healthd interface defines the interaction with the
// platform's cros_healthd.
class CrosHealthdInterface {
 public:
  CrosHealthdInterface(const CrosHealthdInterface&) = delete;
  CrosHealthdInterface& operator=(const CrosHealthdInterface&) = delete;

  virtual ~CrosHealthdInterface() = default;

  // Bootstraps connection to `cros_healthd` mojo from DBus.
  // Also waits for `cros_healthd` service to be available.
  // Must be called prior to using any `cros_healthd` DBus method invocations.
  // Returns true on success into the callback.
  using BootstrapMojoCallback = base::OnceCallback<void(bool)>;
  virtual void BootstrapMojo(BootstrapMojoCallback callback) = 0;

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
