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
  // TODO(crbug.com/1278079): Check for update but skip applying when consumer
  // update is disabled. This will require adding fields to UpdateCheckParams.
  DevicePolicyProvider* const dp_provider = state->device_policy_provider();
  UpdateCheckParams* result =
      UpdateCheckAllowedPolicyData::GetUpdateCheckParams(data);

  // Skip check if device is managed.
  const bool* has_owner_p = ec->GetValue(dp_provider->var_has_owner());
  if (has_owner_p && !(*has_owner_p)) {
    LOG(INFO) << "Managed device, ignoring consumer auto update.";
    return EvalStatus::kContinue;
  }

  // Otherwise, check if the consumer device has auto updates disabled.
  const bool* updater_consumer_auto_update_disabled_p = ec->GetValue(
      state->updater_provider()->var_consumer_auto_update_disabled());
  if (updater_consumer_auto_update_disabled_p) {
    // Auto update is enabled.
    if (!(*updater_consumer_auto_update_disabled_p)) {
      LOG(INFO) << "Consumer auto update is enabled.";
      return EvalStatus::kContinue;
    }

    // Auto update is disabled.

    // If interactive, ignore the disabled consumer auto update.
    // This is a safety check.
    if (!result->interactive) {
      LOG(INFO) << "Disabled consumer auto update.";
      return EvalStatus::kAskMeAgainLater;
    }
    LOG(INFO) << "Disabled consumer auto update, "
              << "but continuing as interactive.";
  }

  LOG(WARNING) << "Couldn't find consumer auto update value.";
  return EvalStatus::kContinue;
}

}  // namespace chromeos_update_manager
