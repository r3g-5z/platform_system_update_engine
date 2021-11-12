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

#include "update_engine/update_manager/consumer_auto_update_policy_impl.h"

#include "update_engine/common/constants.h"
#include "update_engine/common/system_state.h"
#include "update_engine/update_manager/update_check_allowed_policy_data.h"

#include <base/logging.h>

namespace chromeos_update_manager {

// Do not perform any updates if consumer has disabled auto updates.
// However allow interactive updates to continue.
EvalStatus ConsumerAutoUpdatePolicyImpl::Evaluate(
    EvaluationContext* ec,
    State* state,
    std::string* error,
    PolicyDataInterface* data) const {
  DevicePolicyProvider* const dp_provider = state->device_policy_provider();
  UpdateCheckParams* result =
      UpdateCheckAllowedPolicyData::GetUpdateCheckParams(data);

  // Skip check if device is managed.
  const bool* device_policy_is_loaded_p =
      ec->GetValue(dp_provider->var_device_policy_is_loaded());
  if (device_policy_is_loaded_p && *device_policy_is_loaded_p) {
    return EvalStatus::kContinue;
  }

  // Otherwise, check if the consumer device has auto updates disabled.
  if (chromeos_update_engine::SystemState::Get()->prefs()->Exists(
          chromeos_update_engine::kPrefsDisableConsumerAutoUpdate)) {
    // If interactive, ignore the disabled consumer auto update.
    // This is a safety check.
    if (!result->interactive) {
      LOG(INFO) << "Disabled consumer auto update.";
      return EvalStatus::kAskMeAgainLater;
    }
    LOG(INFO) << "Disabled consumer auto update, "
              << "but continuing as interactive.";
  }

  return EvalStatus::kContinue;
}

}  // namespace chromeos_update_manager
