//
// Copyright 2021 The Android Open Source Project
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

#include "update_engine/cros/fake_system_state.h"
#include "update_engine/update_manager/consumer_auto_update_policy_impl.h"
#include "update_engine/update_manager/policy_test_utils.h"

using chromeos_update_engine::FakeSystemState;
using testing::_;
using testing::Return;

namespace chromeos_update_manager {

class UmConsumerAutoUpdatePolicyImplTest : public UmPolicyTestBase {
 protected:
  UmConsumerAutoUpdatePolicyImplTest() : UmPolicyTestBase() {
    policy_data_.reset(new UpdateCheckAllowedPolicyData());
    policy_2_.reset(new ConsumerAutoUpdatePolicyImpl());

    ucp_ =
        UpdateCheckAllowedPolicyData::GetUpdateCheckParams(policy_data_.get());
  }

  void SetUp() override {
    UmPolicyTestBase::SetUp();
    FakeSystemState::CreateInstance();
    FakeSystemState::Get()->set_prefs(nullptr);
  }

  UpdateCheckParams* ucp_;
};

TEST_F(UmConsumerAutoUpdatePolicyImplTest, SkipIfDevicePolicyExists) {
  fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
      new bool(true));
  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
}

TEST_F(UmConsumerAutoUpdatePolicyImplTest, SkipIfNotDisabled) {
  fake_state_.device_policy_provider()->var_has_owner()->reset(new bool(false));
  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
}

TEST_F(UmConsumerAutoUpdatePolicyImplTest, ConsumerDeviceEnabledAutoUpdate) {
  fake_state_.device_policy_provider()->var_has_owner()->reset(new bool(true));
  fake_state_.updater_provider()->var_consumer_auto_update_disabled()->reset(
      new bool(false));
  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
}

TEST_F(UmConsumerAutoUpdatePolicyImplTest,
       ConsumerDeviceDisabledAutoUpdateBackgroundCheck) {
  fake_state_.device_policy_provider()->var_has_owner()->reset(new bool(true));
  fake_state_.updater_provider()->var_consumer_auto_update_disabled()->reset(
      new bool(true));
  ucp_->interactive = false;
  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
}

TEST_F(UmConsumerAutoUpdatePolicyImplTest,
       ConsumerDeviceDisabledAutoUpdateInteractiveCheck) {
  fake_state_.device_policy_provider()->var_has_owner()->reset(new bool(true));
  fake_state_.updater_provider()->var_consumer_auto_update_disabled()->reset(
      new bool(false));
  ucp_->interactive = true;
  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
}

TEST_F(UmConsumerAutoUpdatePolicyImplTest, ManagedDeviceContinues) {
  fake_state_.device_policy_provider()->var_has_owner()->reset(new bool(false));
  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
}

}  // namespace chromeos_update_manager
