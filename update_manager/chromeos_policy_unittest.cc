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

#include "update_engine/cros/fake_system_state.h"
// TODO(b/179419726): Remove.
#include "update_engine/update_manager/enterprise_device_policy_impl.h"
#include "update_engine/update_manager/next_update_check_policy_impl.h"
// TODO(b/179419726): Remove.
#include "update_engine/update_manager/p2p_enabled_policy.h"
#include "update_engine/update_manager/policy_test_utils.h"
#include "update_engine/update_manager/update_can_be_applied_policy_data.h"
#include "update_engine/update_manager/update_can_start_policy.h"
#include "update_engine/update_manager/update_check_allowed_policy.h"
#include "update_engine/update_manager/update_check_allowed_policy_data.h"
#include "update_engine/update_manager/update_time_restrictions_policy_impl.h"
#include "update_engine/update_manager/weekly_time.h"

using base::Time;
using base::TimeDelta;
using std::string;

namespace chromeos_update_manager {

// TODO(b/179419726): Rename this class to |UpdateCheckAllowedPolicyTest|.
class UmChromeOSPolicyTest : public UmPolicyTestBase {
 protected:
  UmChromeOSPolicyTest() : UmPolicyTestBase() {
    policy_data_.reset(new UpdateCheckAllowedPolicyData());
    policy_2_.reset(new UpdateCheckAllowedPolicy());

    uca_data_ = static_cast<typeof(uca_data_)>(policy_data_.get());
  }

  void SetUp() override {
    UmPolicyTestBase::SetUp();
    SetUpDefaultDevicePolicy();
  }

  void SetUpDefaultState() override {
    UmPolicyTestBase::SetUpDefaultState();

    // OOBE is enabled by default.
    fake_state_.config_provider()->var_is_oobe_enabled()->reset(new bool(true));

    // For the purpose of the tests, this is an official build and OOBE was
    // completed.
    fake_state_.system_provider()->var_is_official_build()->reset(
        new bool(true));
    fake_state_.system_provider()->var_is_oobe_complete()->reset(
        new bool(true));
    // NOLINTNEXTLINE(readability/casting)
    fake_state_.system_provider()->var_num_slots()->reset(new unsigned int(2));
  }

  // Sets up a default device policy that does not impose any restrictions
  // (HTTP) nor enables any features (P2P).
  void SetUpDefaultDevicePolicy() {
    fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
        new bool(true));
    fake_state_.device_policy_provider()->var_update_disabled()->reset(
        new bool(false));
    fake_state_.device_policy_provider()
        ->var_allowed_connection_types_for_update()
        ->reset(nullptr);
    fake_state_.device_policy_provider()->var_scatter_factor()->reset(
        new TimeDelta());
    fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
        new bool(true));
    fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
        new bool(false));
    fake_state_.device_policy_provider()
        ->var_release_channel_delegated()
        ->reset(new bool(true));
    fake_state_.device_policy_provider()
        ->var_disallowed_time_intervals()
        ->reset(new WeeklyTimeIntervalVector());
  }

  // Configures the policy to return a desired value from UpdateCheckAllowed by
  // faking the current wall clock time as needed. Restores the default state.
  // This is used when testing policies that depend on this one.
  //
  // Note that the default implementation relies on NextUpdateCheckPolicyImpl to
  // set the FakeClock to the appropriate time.
  virtual void SetUpdateCheckAllowed(bool allow_check) {
    Time next_update_check;
    CallMethodWithContext(&NextUpdateCheckTimePolicyImpl::NextUpdateCheckTime,
                          &next_update_check,
                          kNextUpdateCheckPolicyConstants);
    SetUpDefaultState();
    SetUpDefaultDevicePolicy();
    Time curr_time = next_update_check;
    if (allow_check)
      curr_time += TimeDelta::FromSeconds(1);
    else
      curr_time -= TimeDelta::FromSeconds(1);
    fake_clock_->SetWallclockTime(curr_time);
  }

  UpdateCheckAllowedPolicyData* uca_data_;
};

// TODO(b/179419726): Merge into enterprise_device_policy_impl_unittest.cc.
class UmEnterprisePolicyTest : public UmPolicyTestBase {
 protected:
  UmEnterprisePolicyTest() : UmPolicyTestBase() {
    policy_data_.reset(new UpdateCheckAllowedPolicyData());
    policy_2_.reset(new EnterpriseDevicePolicyImpl());

    uca_data_ = static_cast<typeof(uca_data_)>(policy_data_.get());
  }

  void SetUp() override {
    UmPolicyTestBase::SetUp();
    fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
        new bool(true));
  }

  // Sets the policies required for a kiosk app to control Chrome OS version:
  // - AllowKioskAppControlChromeVersion = True
  // - UpdateDisabled = True
  // In the kiosk app manifest:
  // - RequiredPlatformVersion = 1234.
  void SetKioskAppControlsChromeOsVersion() {
    fake_state_.device_policy_provider()
        ->var_allow_kiosk_app_control_chrome_version()
        ->reset(new bool(true));
    fake_state_.device_policy_provider()->var_update_disabled()->reset(
        new bool(true));
    fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
        new string("1234."));
  }

  // Sets up a test with the value of RollbackToTargetVersion policy (and
  // whether it's set), and returns the value of
  // UpdateCheckParams.rollback_allowed.
  bool TestRollbackAllowed(bool set_policy,
                           RollbackToTargetVersion rollback_to_target_version) {
    if (set_policy) {
      // Override RollbackToTargetVersion device policy attribute.
      fake_state_.device_policy_provider()
          ->var_rollback_to_target_version()
          ->reset(new RollbackToTargetVersion(rollback_to_target_version));
    }

    EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
    return uca_data_->update_check_params.rollback_allowed;
  }

  UpdateCheckAllowedPolicyData* uca_data_;
};

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedWaitsForTheTimeout) {
  // We get the next update_check timestamp from the policy's private method
  // and then we check the public method respects that value on the normal
  // case.
  Time next_update_check;
  Time last_checked_time =
      fake_clock_->GetWallclockTime() + TimeDelta::FromMinutes(1234);

  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  CallMethodWithContext(&NextUpdateCheckTimePolicyImpl::NextUpdateCheckTime,
                        &next_update_check,
                        kNextUpdateCheckPolicyConstants);

  // Check that the policy blocks until the next_update_check is reached.
  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_->SetWallclockTime(next_update_check - TimeDelta::FromSeconds(1));

  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());

  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_->SetWallclockTime(next_update_check + TimeDelta::FromSeconds(1));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(uca_data_->update_check_params.updates_enabled);
  EXPECT_FALSE(uca_data_->update_check_params.interactive);
}

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedWaitsForOOBE) {
  // Update checks are deferred until OOBE is completed.

  // Ensure that update is not allowed even if wait period is satisfied.
  Time next_update_check;
  Time last_checked_time =
      fake_clock_->GetWallclockTime() + TimeDelta::FromMinutes(1234);

  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  CallMethodWithContext(&NextUpdateCheckTimePolicyImpl::NextUpdateCheckTime,
                        &next_update_check,
                        kNextUpdateCheckPolicyConstants);

  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_->SetWallclockTime(next_update_check + TimeDelta::FromSeconds(1));
  fake_state_.system_provider()->var_is_oobe_complete()->reset(new bool(false));

  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());

  // Now check that it is allowed if OOBE is completed.
  SetUpDefaultClock();
  SetUpDefaultState();
  fake_state_.updater_provider()->var_last_checked_time()->reset(
      new Time(last_checked_time));
  fake_clock_->SetWallclockTime(next_update_check + TimeDelta::FromSeconds(1));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(uca_data_->update_check_params.updates_enabled);
  EXPECT_FALSE(uca_data_->update_check_params.interactive);
}

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedWithAttributes) {
  // Update check is allowed, response includes attributes for use in the
  // request.
  SetUpdateCheckAllowed(true);

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_target_version_prefix()->reset(
      new string("1.2"));
  fake_state_.device_policy_provider()
      ->var_rollback_allowed_milestones()
      ->reset(new int(5));
  fake_state_.device_policy_provider()->var_release_channel_delegated()->reset(
      new bool(false));
  fake_state_.device_policy_provider()->var_release_channel()->reset(
      new string("foo-channel"));
  fake_state_.device_policy_provider()->var_release_lts_tag()->reset(
      new string("foo-hint"));
  fake_state_.device_policy_provider()->var_quick_fix_build_token()->reset(
      new string("foo-token"));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(uca_data_->update_check_params.updates_enabled);
  EXPECT_EQ("1.2", uca_data_->update_check_params.target_version_prefix);
  EXPECT_EQ(5, uca_data_->update_check_params.rollback_allowed_milestones);
  EXPECT_EQ("foo-channel", uca_data_->update_check_params.target_channel);
  EXPECT_EQ("foo-hint", uca_data_->update_check_params.lts_tag);
  EXPECT_EQ("foo-token", uca_data_->update_check_params.quick_fix_build_token);
  EXPECT_FALSE(uca_data_->update_check_params.interactive);
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedRollbackAndPowerwash) {
  EXPECT_TRUE(TestRollbackAllowed(
      true, RollbackToTargetVersion::kRollbackAndPowerwash));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedRollbackAndRestoreIfPossible) {
  // We're doing rollback even if we don't support data save and restore.
  EXPECT_TRUE(TestRollbackAllowed(
      true, RollbackToTargetVersion::kRollbackAndRestoreIfPossible));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedRollbackDisabled) {
  EXPECT_FALSE(TestRollbackAllowed(true, RollbackToTargetVersion::kDisabled));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedRollbackUnspecified) {
  EXPECT_FALSE(
      TestRollbackAllowed(true, RollbackToTargetVersion::kUnspecified));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedRollbackNotSet) {
  EXPECT_FALSE(
      TestRollbackAllowed(false, RollbackToTargetVersion::kUnspecified));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedKioskRollbackAllowed) {
  SetKioskAppControlsChromeOsVersion();

  EXPECT_TRUE(TestRollbackAllowed(
      true, RollbackToTargetVersion::kRollbackAndPowerwash));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedKioskRollbackDisabled) {
  SetKioskAppControlsChromeOsVersion();

  EXPECT_FALSE(TestRollbackAllowed(true, RollbackToTargetVersion::kDisabled));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedKioskRollbackUnspecified) {
  SetKioskAppControlsChromeOsVersion();

  EXPECT_FALSE(
      TestRollbackAllowed(true, RollbackToTargetVersion::kUnspecified));
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedKioskRollbackNotSet) {
  SetKioskAppControlsChromeOsVersion();

  EXPECT_FALSE(
      TestRollbackAllowed(false, RollbackToTargetVersion::kUnspecified));
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCheckAllowedUpdatesDisabledForUnofficialBuilds) {
  // UpdateCheckAllowed should return kAskMeAgainLater if this is an unofficial
  // build; we don't want periodic update checks on developer images.

  fake_state_.system_provider()->var_is_official_build()->reset(
      new bool(false));

  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
}

TEST_F(UmChromeOSPolicyTest, TestUpdateCheckIntervalTimeout) {
  fake_state_.updater_provider()
      ->var_test_update_check_interval_timeout()
      ->reset(new int64_t(10));
  fake_state_.system_provider()->var_is_official_build()->reset(
      new bool(false));

  // The first time, update should not be allowed.
  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());

  // After moving the time forward more than the update check interval, it
  // should now allow for update.
  fake_clock_->SetWallclockTime(fake_clock_->GetWallclockTime() +
                                TimeDelta::FromSeconds(11));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCheckAllowedUpdatesDisabledWhenNotEnoughSlotsAbUpdates) {
  // UpdateCheckAllowed should return false (kSucceeded) if the image booted
  // without enough slots to do A/B updates.

  // NOLINTNEXTLINE(readability/casting)
  fake_state_.system_provider()->var_num_slots()->reset(new unsigned int(1));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_FALSE(uca_data_->update_check_params.updates_enabled);
}

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedUpdatesDisabledByPolicy) {
  // UpdateCheckAllowed should return kAskMeAgainLater because a device policy
  // is loaded and prohibits updates.

  SetUpdateCheckAllowed(false);
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));

  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
}

TEST_F(UmChromeOSPolicyTest,
       UpdateCheckAllowedForcedUpdateRequestedInteractive) {
  // UpdateCheckAllowed should return true because a forced update request was
  // signaled for an interactive update.

  SetUpdateCheckAllowed(true);
  fake_state_.updater_provider()->var_forced_update_requested()->reset(
      new UpdateRequestStatus(UpdateRequestStatus::kInteractive));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(uca_data_->update_check_params.updates_enabled);
  EXPECT_TRUE(uca_data_->update_check_params.interactive);
}

TEST_F(UmChromeOSPolicyTest, UpdateCheckAllowedForcedUpdateRequestedPeriodic) {
  // UpdateCheckAllowed should return true because a forced update request was
  // signaled for a periodic check.

  SetUpdateCheckAllowed(true);
  fake_state_.updater_provider()->var_forced_update_requested()->reset(
      new UpdateRequestStatus(UpdateRequestStatus::kPeriodic));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(uca_data_->update_check_params.updates_enabled);
  EXPECT_FALSE(uca_data_->update_check_params.interactive);
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedKioskPin) {
  SetKioskAppControlsChromeOsVersion();

  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
  EXPECT_TRUE(uca_data_->update_check_params.updates_enabled);
  EXPECT_EQ("1234.", uca_data_->update_check_params.target_version_prefix);
  EXPECT_FALSE(uca_data_->update_check_params.interactive);
}

TEST_F(UmEnterprisePolicyTest, UpdateCheckAllowedDisabledWhenNoKioskPin) {
  // Disable AU policy is set but kiosk pin policy is set to false. Update is
  // disabled in such case.
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(false));

  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
}

TEST_F(UmEnterprisePolicyTest,
       UpdateCheckAllowedKioskPinWithNoRequiredVersion) {
  // AU disabled, allow kiosk to pin but there is no kiosk required platform
  // version (i.e. app does not provide the info). Update to latest in such
  // case.
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(true));
  fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
      new string());

  EXPECT_EQ(EvalStatus::kContinue, evaluator_->Evaluate());
  EXPECT_TRUE(uca_data_->update_check_params.updates_enabled);
  EXPECT_TRUE(uca_data_->update_check_params.target_version_prefix.empty());
  EXPECT_FALSE(uca_data_->update_check_params.interactive);
}

TEST_F(UmEnterprisePolicyTest,
       UpdateCheckAllowedKioskPinWithFailedGetRequiredVersionCall) {
  // AU disabled, allow kiosk to pin but D-Bus call to get required platform
  // version failed. Defer update check in this case.
  fake_state_.device_policy_provider()->var_update_disabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()
      ->var_allow_kiosk_app_control_chrome_version()
      ->reset(new bool(true));
  fake_state_.system_provider()->var_kiosk_required_platform_version()->reset(
      nullptr);

  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
}

class UmP2PEnabledPolicyTest : public UmPolicyTestBase {
 protected:
  UmP2PEnabledPolicyTest() : UmPolicyTestBase() {
    policy_data_.reset(new P2PEnabledPolicyData());
    policy_2_.reset(new P2PEnabledPolicy());

    p2p_data_ = static_cast<typeof(p2p_data_)>(policy_data_.get());
  }

  void SetUp() override {
    UmPolicyTestBase::SetUp();
    fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
        new bool(true));
    fake_state_.device_policy_provider()->var_has_owner()->reset(
        new bool(true));
  }

  P2PEnabledPolicyData* p2p_data_;
};

TEST_F(UmP2PEnabledPolicyTest, NotAllowed) {
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_FALSE(p2p_data_->enabled());
}

TEST_F(UmP2PEnabledPolicyTest, AllowedByDevicePolicy) {
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(p2p_data_->enabled());
}

TEST_F(UmP2PEnabledPolicyTest, AllowedByUpdater) {
  fake_state_.updater_provider()->var_p2p_enabled()->reset(new bool(true));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(p2p_data_->enabled());
}

TEST_F(UmP2PEnabledPolicyTest, DeviceEnterpriseEnrolled) {
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(nullptr);
  fake_state_.device_policy_provider()->var_has_owner()->reset(new bool(false));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(p2p_data_->enabled());
}

class UmP2PEnabledChangedPolicyTest : public UmPolicyTestBase {
 protected:
  UmP2PEnabledChangedPolicyTest() : UmPolicyTestBase() {
    policy_data_.reset(new P2PEnabledPolicyData());
    policy_2_.reset(new P2PEnabledChangedPolicy());
  }
};

TEST_F(UmP2PEnabledChangedPolicyTest, Blocks) {
  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
}

}  // namespace chromeos_update_manager
