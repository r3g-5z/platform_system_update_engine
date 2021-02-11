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

#include <memory>

#include <gtest/gtest.h>

#include "update_engine/update_manager/policy_test_utils.h"
#include "update_engine/update_manager/recovery_policy.h"

namespace chromeos_update_manager {

class UmRecoveryPolicyTest : public UmPolicyTestBase {
 protected:
  UmRecoveryPolicyTest() : UmPolicyTestBase() {
    policy_ = std::make_unique<RecoveryPolicy>();
  }
};

TEST_F(UmRecoveryPolicyTest, RecoveryMode) {
  fake_state_.updater_provider()->var_running_from_minios()->reset(
      new bool(true));
  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kSucceeded, &Policy::UpdateCheckAllowed, &result);
}

TEST_F(UmRecoveryPolicyTest, NotRecoveryMode) {
  fake_state_.updater_provider()->var_running_from_minios()->reset(
      new bool(false));
  UpdateCheckParams result;
  ExpectPolicyStatus(
      EvalStatus::kContinue, &Policy::UpdateCheckAllowed, &result);
}

}  // namespace chromeos_update_manager
