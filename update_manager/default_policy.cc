//
// Copyright (C) 2014 The Android Open Source Project
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

#include "update_engine/common/system_state.h"
// TODO(b/179419726): Remove.
#include "update_engine/update_manager/update_can_be_applied_policy.h"
#include "update_engine/update_manager/update_can_be_applied_policy_data.h"
#include "update_engine/update_manager/update_check_allowed_policy.h"

using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::SystemState;
using std::string;

namespace chromeos_update_manager {

namespace {

// A fixed minimum interval between consecutive allowed update checks. This
// needs to be long enough to prevent busywork and/or DDoS attacks on Omaha, but
// at the same time short enough to allow the machine to update itself
// reasonably soon.
const int kCheckIntervalInSeconds = 15 * 60;

}  // namespace

// TODO(b/179419726): Move to update_check_allowed_policy.cc.
EvalStatus UpdateCheckAllowedPolicy::EvaluateDefault(
    EvaluationContext* ec,
    State* state,
    string* error,
    PolicyDataInterface* data) const {
  UpdateCheckParams* result =
      UpdateCheckAllowedPolicyData::GetUpdateCheckParams(data);
  result->updates_enabled = true;
  result->target_channel.clear();
  result->lts_tag.clear();
  result->target_version_prefix.clear();
  result->rollback_allowed = false;
  result->rollback_allowed_milestones = -1;  // No version rolls should happen.
  result->rollback_on_channel_downgrade = false;
  result->interactive = false;
  result->quick_fix_build_token.clear();

  // Ensure that the minimum interval is set. If there's no clock, this defaults
  // to always allowing the update.
  if (!aux_state_->IsLastCheckAllowedTimeSet() ||
      ec->IsMonotonicTimeGreaterThan(
          aux_state_->last_check_allowed_time() +
          base::TimeDelta::FromSeconds(kCheckIntervalInSeconds))) {
    aux_state_->set_last_check_allowed_time(
        SystemState::Get()->clock()->GetMonotonicTime());
    return EvalStatus::kSucceeded;
  }

  return EvalStatus::kAskMeAgainLater;
}

// TODO(b/179419726): Move to update_can_be_applied.cc.
EvalStatus UpdateCanBeAppliedPolicy::EvaluateDefault(
    EvaluationContext* ec,
    State* state,
    std::string* error,
    PolicyDataInterface* data) const {
  static_cast<UpdateCanBeAppliedPolicyData*>(data)->set_error_code(
      ErrorCode::kSuccess);
  return EvalStatus::kSucceeded;
}

}  // namespace chromeos_update_manager
