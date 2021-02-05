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
#include "update_engine/update_manager/update_check_allowed_policy_data.h"

namespace chromeos_update_manager {

class UmRecoveryPolicyTest : public UmPolicyTestBase {
 protected:
  UmRecoveryPolicyTest() : UmPolicyTestBase() {
    policy_data_.reset(new UpdateCheckAllowedPolicyData());
    policy_2_.reset(new RecoveryPolicy());
  }
};

TEST_F(UmRecoveryPolicyTest, RecoveryMode) {
  fake_state_.updater_provider()->var_running_from_minios()->reset(
      new bool(true));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
}

TEST_F(UmRecoveryPolicyTest, NotRecoveryMode) {
  fake_state_.updater_provider()->var_running_from_minios()->reset(
      new bool(false));
  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
}

}  // namespace chromeos_update_manager
