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

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>

#include "update_engine/common/error_code.h"
#include "update_engine/common/error_code_utils.h"
#include "update_engine/common/system_state.h"
#include "update_engine/common/utils.h"
#include "update_engine/update_manager/device_policy_provider.h"
#include "update_engine/update_manager/enough_slots_ab_updates_policy_impl.h"
#include "update_engine/update_manager/enterprise_device_policy_impl.h"
#include "update_engine/update_manager/enterprise_rollback_policy_impl.h"
#include "update_engine/update_manager/interactive_update_policy_impl.h"
#include "update_engine/update_manager/minimum_version_policy_impl.h"
#include "update_engine/update_manager/next_update_check_policy_impl.h"
#include "update_engine/update_manager/official_build_check_policy_impl.h"
#include "update_engine/update_manager/out_of_box_experience_policy_impl.h"
#include "update_engine/update_manager/policy_utils.h"
#include "update_engine/update_manager/recovery_policy.h"
#include "update_engine/update_manager/shill_provider.h"
// TODO(b/179419726): Remove.
#include "update_engine/update_manager/update_can_be_applied_policy.h"
#include "update_engine/update_manager/update_can_be_applied_policy_data.h"
#include "update_engine/update_manager/update_check_allowed_policy.h"
#include "update_engine/update_manager/update_time_restrictions_policy_impl.h"

using base::Time;
using base::TimeDelta;
using chromeos_update_engine::ConnectionTethering;
using chromeos_update_engine::ConnectionType;
using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::InstallPlan;
using chromeos_update_engine::SystemState;
using std::string;
using std::vector;

namespace chromeos_update_manager {

// TODO(b/179419726): Move to update_check_allowed_policy.cc.
EvalStatus UpdateCheckAllowedPolicy::Evaluate(EvaluationContext* ec,
                                              State* state,
                                              string* error,
                                              PolicyDataInterface* data) const {
  UpdateCheckParams* result =
      UpdateCheckAllowedPolicyData::GetUpdateCheckParams(data);
  // Set the default return values.
  result->updates_enabled = true;
  result->target_channel.clear();
  result->lts_tag.clear();
  result->target_version_prefix.clear();
  result->rollback_allowed = false;
  result->rollback_allowed_milestones = -1;
  result->rollback_on_channel_downgrade = false;
  result->interactive = false;
  result->quick_fix_build_token.clear();

  RecoveryPolicy recovery_policy;
  EnoughSlotsAbUpdatesPolicyImpl enough_slots_ab_updates_policy;
  EnterpriseDevicePolicyImpl enterprise_device_policy;
  OnlyUpdateOfficialBuildsPolicyImpl only_update_official_builds_policy;
  InteractiveUpdateCheckAllowedPolicyImpl interactive_update_policy;
  OobePolicyImpl oobe_policy;
  NextUpdateCheckTimePolicyImpl next_update_check_time_policy;

  vector<PolicyInterface* const> policies_to_consult = {
      // If in recovery mode, always check for update.
      &recovery_policy,

      // Do not perform any updates if there are not enough slots to do A/B
      // updates.
      &enough_slots_ab_updates_policy,

      // Check to see if Enterprise-managed (has DevicePolicy) and/or
      // Kiosk-mode.  If so, then defer to those settings.
      &enterprise_device_policy,

      // Check to see if an interactive update was requested.
      &interactive_update_policy,

      // Unofficial builds should not perform periodic update checks.
      &only_update_official_builds_policy,

      // If OOBE is enabled, wait until it is completed.
      &oobe_policy,

      // Ensure that periodic update checks are timed properly.
      &next_update_check_time_policy,
  };

  // Now that the list of policy implementations, and the order to consult them,
  // has been setup, consult the policies. If none of the policies make a
  // definitive decisions about whether or not to check for updates, then allow
  // the update check to happen.
  for (auto policy : policies_to_consult) {
    EvalStatus status = policy->Evaluate(ec, state, error, data);
    if (status != EvalStatus::kContinue) {
      return status;
    }
  }
  LOG(INFO) << "Allowing update check.";
  return EvalStatus::kSucceeded;
}

EvalStatus UpdateCanBeAppliedPolicy::Evaluate(EvaluationContext* ec,
                                              State* state,
                                              std::string* error,
                                              PolicyDataInterface* data) const {
  InteractiveUpdateCanBeAppliedPolicyImpl interactive_update_policy;
  EnterpriseRollbackPolicyImpl enterprise_rollback_policy;
  MinimumVersionPolicyImpl minimum_version_policy;
  UpdateTimeRestrictionsPolicyImpl update_time_restrictions_policy;

  vector<PolicyInterface const*> policies_to_consult = {
      // Check to see if an interactive update has been requested.
      &interactive_update_policy,

      // Check whether current update is enterprise rollback.
      &enterprise_rollback_policy,

      // Check whether update happens from a version less than the minimum
      // required one.
      &minimum_version_policy,

      // Do not apply or download an update if we are inside one of the
      // restricted times.
      &update_time_restrictions_policy,
  };

  for (auto policy : policies_to_consult) {
    EvalStatus status = policy->Evaluate(ec, state, error, data);
    if (status != EvalStatus::kContinue) {
      return status;
    }
  }
  LOG(INFO) << "Allowing update to be applied.";
  static_cast<UpdateCanBeAppliedPolicyData*>(data)->set_error_code(
      ErrorCode::kSuccess);
  return EvalStatus::kSucceeded;
}

}  // namespace chromeos_update_manager
