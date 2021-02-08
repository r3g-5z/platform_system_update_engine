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

#include "update_engine/update_manager/chromeos_policy.h"

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

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
using chromeos_update_engine::ConnectionTethering;
using chromeos_update_engine::ConnectionType;
using chromeos_update_engine::ErrorCode;
using chromeos_update_engine::FakeSystemState;
using chromeos_update_engine::InstallPlan;
using std::set;
using std::string;
using std::tuple;
using std::vector;

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

    // Connection is wifi, untethered.
    fake_state_.shill_provider()->var_conn_type()->reset(
        new ConnectionType(ConnectionType::kWifi));
    fake_state_.shill_provider()->var_conn_tethering()->reset(
        new ConnectionTethering(ConnectionTethering::kNotDetected));
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

class UmUpdateCanStartPolicyTest : public UmPolicyTestBase {
 protected:
  UmUpdateCanStartPolicyTest() : UmPolicyTestBase() {
    policy_data_.reset(new UpdateCanStartPolicyData());
    policy_2_.reset(new UpdateCanStartPolicy());

    ucs_data_ = static_cast<typeof(ucs_data_)>(policy_data_.get());
  }

  void SetUp() override {
    UmPolicyTestBase::SetUp();
    SetUpDefaultState();
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

    // Connection is wifi, untethered.
    fake_state_.shill_provider()->var_conn_type()->reset(
        new ConnectionType(ConnectionType::kWifi));
    fake_state_.shill_provider()->var_conn_tethering()->reset(
        new ConnectionTethering(ConnectionTethering::kNotDetected));
  }

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

  // Returns a default UpdateState structure:
  UpdateState GetDefaultUpdateState(TimeDelta first_seen_period) {
    Time first_seen_time =
        FakeSystemState::Get()->clock()->GetWallclockTime() - first_seen_period;
    UpdateState update_state = UpdateState();

    // This is a non-interactive check returning a delta payload, seen for the
    // first time (|first_seen_period| ago). Clearly, there were no failed
    // attempts so far.
    update_state.interactive = false;
    update_state.is_delta_payload = false;
    update_state.first_seen = first_seen_time;
    update_state.num_checks = 1;
    update_state.num_failures = 0;
    update_state.failures_last_updated = Time();  // Needs to be zero.
    // There's a single HTTP download URL with a maximum of 10 retries.
    update_state.download_urls = vector<string>{"http://fake/url/"};
    update_state.download_errors_max = 10;
    // Download was never attempted.
    update_state.last_download_url_idx = -1;
    update_state.last_download_url_num_errors = 0;
    // There were no download errors.
    update_state.download_errors = vector<tuple<int, ErrorCode, Time>>();
    // P2P is not disabled by Omaha.
    update_state.p2p_downloading_disabled = false;
    update_state.p2p_sharing_disabled = false;
    // P2P was not attempted.
    update_state.p2p_num_attempts = 0;
    update_state.p2p_first_attempted = Time();
    // No active backoff period, backoff is not disabled by Omaha.
    update_state.backoff_expiry = Time();
    update_state.is_backoff_disabled = false;
    // There is no active scattering wait period (max 7 days allowed) nor check
    // threshold (none allowed).
    update_state.scatter_wait_period = TimeDelta();
    update_state.scatter_check_threshold = 0;
    update_state.scatter_wait_period_max = TimeDelta::FromDays(7);
    update_state.scatter_check_threshold_min = 0;
    update_state.scatter_check_threshold_max = 0;

    return update_state;
  }

  UpdateCanStartPolicyData* ucs_data_;
};

TEST_F(UmUpdateCanStartPolicyTest, AllowedNoDevicePolicy) {
  // The UpdateCanStart policy returns true; no device policy is loaded.

  fake_state_.device_policy_provider()->var_device_policy_is_loaded()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns true with no further attributes.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_FALSE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_FALSE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedBlankPolicy) {
  // The UpdateCanStart policy returns true; device policy is loaded but imposes
  // no restrictions on updating.

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_FALSE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_FALSE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, NotAllowedBackoffNewWaitPeriodApplies) {
  // The UpdateCanStart policy returns false; failures are reported and a new
  // backoff period is enacted.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));

  // Check that UpdateCanStart returns false and a new backoff expiry is
  // generated.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_FALSE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kBackoff,
            ucs_data_->result.cannot_start_reason);
  EXPECT_TRUE(ucs_data_->result.do_increment_failures);
  EXPECT_LT(curr_time, ucs_data_->result.backoff_expiry);
}

TEST_F(UmUpdateCanStartPolicyTest,
       NotAllowedBackoffPrevWaitPeriodStillApplies) {
  // The UpdateCanStart policy returns false; a previously enacted backoff
  // period still applies.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));
  ucs_data_->update_state.failures_last_updated = curr_time;
  ucs_data_->update_state.backoff_expiry =
      curr_time + TimeDelta::FromMinutes(3);

  // Check that UpdateCanStart returns false and a new backoff expiry is
  // generated.
  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
  EXPECT_FALSE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kBackoff,
            ucs_data_->result.cannot_start_reason);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
  EXPECT_LT(curr_time, ucs_data_->result.backoff_expiry);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedBackoffSatisfied) {
  // The UpdateCanStart policy returns true; a previously enacted backoff period
  // has elapsed, we're good to go.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));
  ucs_data_->update_state.failures_last_updated =
      curr_time - TimeDelta::FromSeconds(1);
  ucs_data_->update_state.backoff_expiry =
      curr_time - TimeDelta::FromSeconds(1);

  // Check that UpdateCanStart returns false and a new backoff expiry is
  // generated.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kUndefined,
            ucs_data_->result.cannot_start_reason);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
  EXPECT_EQ(Time(), ucs_data_->result.backoff_expiry);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedBackoffDisabled) {
  // The UpdateCanStart policy returns false; failures are reported but backoff
  // is disabled.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));
  ucs_data_->update_state.is_backoff_disabled = true;

  // Check that UpdateCanStart returns false and a new backoff expiry is
  // generated.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kUndefined,
            ucs_data_->result.cannot_start_reason);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_TRUE(ucs_data_->result.do_increment_failures);
  EXPECT_EQ(Time(), ucs_data_->result.backoff_expiry);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedNoBackoffInteractive) {
  // The UpdateCanStart policy returns false; failures are reported but this is
  // an interactive update check.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));
  ucs_data_->update_state.interactive = true;

  // Check that UpdateCanStart returns false and a new backoff expiry is
  // generated.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kUndefined,
            ucs_data_->result.cannot_start_reason);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_TRUE(ucs_data_->result.do_increment_failures);
  EXPECT_EQ(Time(), ucs_data_->result.backoff_expiry);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedNoBackoffDelta) {
  // The UpdateCanStart policy returns false; failures are reported but this is
  // a delta payload.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));
  ucs_data_->update_state.is_delta_payload = true;

  // Check that UpdateCanStart returns false and a new backoff expiry is
  // generated.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kUndefined,
            ucs_data_->result.cannot_start_reason);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_TRUE(ucs_data_->result.do_increment_failures);
  EXPECT_EQ(Time(), ucs_data_->result.backoff_expiry);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedNoBackoffUnofficialBuild) {
  // The UpdateCanStart policy returns false; failures are reported but this is
  // an unofficial build.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));

  fake_state_.system_provider()->var_is_official_build()->reset(
      new bool(false));

  // Check that UpdateCanStart returns false and a new backoff expiry is
  // generated.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kUndefined,
            ucs_data_->result.cannot_start_reason);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_TRUE(ucs_data_->result.do_increment_failures);
  EXPECT_EQ(Time(), ucs_data_->result.backoff_expiry);
}

TEST_F(UmUpdateCanStartPolicyTest, NotAllowedScatteringNewWaitPeriodApplies) {
  // The UpdateCanStart policy returns false; device policy is loaded and
  // scattering applies due to an unsatisfied wait period, which was newly
  // generated.

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromMinutes(2)));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));

  // Check that the UpdateCanStart returns false and a new wait period
  // generated.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_FALSE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering,
            ucs_data_->result.cannot_start_reason);
  EXPECT_LT(TimeDelta(), ucs_data_->result.scatter_wait_period);
  EXPECT_EQ(0, ucs_data_->result.scatter_check_threshold);
}

TEST_F(UmUpdateCanStartPolicyTest,
       NotAllowedScatteringPrevWaitPeriodStillApplies) {
  // The UpdateCanStart policy returns false w/ kAskMeAgainLater; device policy
  // is loaded and a previously generated scattering period still applies, none
  // of the scattering values has changed.

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromMinutes(2)));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  ucs_data_->update_state.scatter_wait_period = TimeDelta::FromSeconds(35);

  // Check that the UpdateCanStart returns false and a new wait period
  // generated.
  EXPECT_EQ(EvalStatus::kAskMeAgainLater, evaluator_->Evaluate());
  EXPECT_FALSE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering,
            ucs_data_->result.cannot_start_reason);
  EXPECT_EQ(TimeDelta::FromSeconds(35), ucs_data_->result.scatter_wait_period);
  EXPECT_EQ(0, ucs_data_->result.scatter_check_threshold);
}

TEST_F(UmUpdateCanStartPolicyTest,
       NotAllowedScatteringNewCountThresholdApplies) {
  // The UpdateCanStart policy returns false; device policy is loaded and
  // scattering applies due to an unsatisfied update check count threshold.
  //
  // This ensures a non-zero check threshold, which may or may not be combined
  // with a non-zero wait period (for which we cannot reliably control).

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  ucs_data_->update_state.scatter_check_threshold_min = 2;
  ucs_data_->update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns false.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_FALSE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering,
            ucs_data_->result.cannot_start_reason);
  EXPECT_LE(2, ucs_data_->result.scatter_check_threshold);
  EXPECT_GE(5, ucs_data_->result.scatter_check_threshold);
}

TEST_F(UmUpdateCanStartPolicyTest,
       NotAllowedScatteringPrevCountThresholdStillApplies) {
  // The UpdateCanStart policy returns false; device policy is loaded and
  // scattering due to a previously generated count threshold still applies.

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  ucs_data_->update_state.scatter_check_threshold = 3;
  ucs_data_->update_state.scatter_check_threshold_min = 2;
  ucs_data_->update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns false.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_FALSE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kScattering,
            ucs_data_->result.cannot_start_reason);
  EXPECT_EQ(3, ucs_data_->result.scatter_check_threshold);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedScatteringSatisfied) {
  // The UpdateCanStart policy returns true; device policy is loaded and
  // scattering is enabled, but both wait period and check threshold are
  // satisfied.

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(120)));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(75));
  ucs_data_->update_state.num_checks = 4;
  ucs_data_->update_state.scatter_wait_period = TimeDelta::FromSeconds(60);
  ucs_data_->update_state.scatter_check_threshold = 3;
  ucs_data_->update_state.scatter_check_threshold_min = 2;
  ucs_data_->update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(TimeDelta(), ucs_data_->result.scatter_wait_period);
  EXPECT_EQ(0, ucs_data_->result.scatter_check_threshold);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedInteractivePreventsScattering) {
  // The UpdateCanStart policy returns true; device policy is loaded and
  // scattering would have applied, except that the update check is interactive
  // and so it is suppressed.

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  ucs_data_->update_state.interactive = true;
  ucs_data_->update_state.scatter_check_threshold = 0;
  ucs_data_->update_state.scatter_check_threshold_min = 2;
  ucs_data_->update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(TimeDelta(), ucs_data_->result.scatter_wait_period);
  EXPECT_EQ(0, ucs_data_->result.scatter_check_threshold);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedOobePreventsScattering) {
  // The UpdateCanStart policy returns true; device policy is loaded and
  // scattering would have applied, except that OOBE was not completed and so it
  // is suppressed.

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromSeconds(1)));
  fake_state_.system_provider()->var_is_oobe_complete()->reset(new bool(false));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  ucs_data_->update_state.interactive = true;
  ucs_data_->update_state.scatter_check_threshold = 0;
  ucs_data_->update_state.scatter_check_threshold_min = 2;
  ucs_data_->update_state.scatter_check_threshold_max = 5;

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(TimeDelta(), ucs_data_->result.scatter_wait_period);
  EXPECT_EQ(0, ucs_data_->result.scatter_check_threshold);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedWithAttributes) {
  // The UpdateCanStart policy returns true; device policy permits both HTTP and
  // P2P updates, as well as a non-empty target channel string.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_TRUE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedWithP2PFromUpdater) {
  // The UpdateCanStart policy returns true; device policy forbids both HTTP and
  // P2P updates, but the updater is configured to allow P2P and overrules the
  // setting.

  // Override specific device policy attributes.
  fake_state_.updater_provider()->var_p2p_enabled()->reset(new bool(true));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_TRUE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedP2PDownloadingBlockedDueToOmaha) {
  // The UpdateCanStart policy returns true; device policy permits HTTP, but
  // policy blocks P2P downloading because Omaha forbids it.  P2P sharing is
  // still permitted.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.p2p_downloading_disabled = true;
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_FALSE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedP2PSharingBlockedDueToOmaha) {
  // The UpdateCanStart policy returns true; device policy permits HTTP, but
  // policy blocks P2P sharing because Omaha forbids it.  P2P downloading is
  // still permitted.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.p2p_sharing_disabled = true;
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_TRUE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_FALSE(ucs_data_->result.p2p_sharing_allowed);
}

TEST_F(UmUpdateCanStartPolicyTest,
       AllowedP2PDownloadingBlockedDueToNumAttempts) {
  // The UpdateCanStart policy returns true; device policy permits HTTP but
  // blocks P2P download, because the max number of P2P downloads have been
  // attempted. P2P sharing is still permitted.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.p2p_num_attempts = kMaxP2PAttempts;
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_FALSE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
}

TEST_F(UmUpdateCanStartPolicyTest,
       AllowedP2PDownloadingBlockedDueToAttemptsPeriod) {
  // The UpdateCanStart policy returns true; device policy permits HTTP but
  // blocks P2P download, because the max period for attempt to download via P2P
  // has elapsed. P2P sharing is still permitted.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.p2p_num_attempts = 1;
  ucs_data_->update_state.p2p_first_attempted =
      fake_clock_->GetWallclockTime() -
      TimeDelta::FromSeconds(kMaxP2PAttemptsPeriodInSeconds + 1);
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_FALSE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedWithHttpUrlForUnofficialBuild) {
  // The UpdateCanStart policy returns true; device policy forbids both HTTP and
  // P2P updates, but marking this an unofficial build overrules the HTTP
  // setting.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));
  fake_state_.system_provider()->var_is_official_build()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedWithHttpsUrl) {
  // The UpdateCanStart policy returns true; device policy forbids both HTTP and
  // P2P updates, but an HTTPS URL is provided and selected for download.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));

  // Add an HTTPS URL.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.download_urls.emplace_back("https://secure/url/");

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(1, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedMaxErrorsNotExceeded) {
  // The UpdateCanStart policy returns true; the first URL has download errors
  // but does not exceed the maximum allowed number of failures, so it is stilli
  // usable.

  // Add a second URL; update with this URL attempted and failed enough times to
  // disqualify the current (first) URL.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.num_checks = 5;
  ucs_data_->update_state.download_urls.emplace_back(
      "http://another/fake/url/");
  Time t = fake_clock_->GetWallclockTime() - TimeDelta::FromSeconds(12);
  for (int i = 0; i < 5; i++) {
    ucs_data_->update_state.download_errors.emplace_back(
        0, ErrorCode::kDownloadTransferError, t);
    t += TimeDelta::FromSeconds(1);
  }

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(5, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedWithSecondUrlMaxExceeded) {
  // The UpdateCanStart policy returns true; the first URL exceeded the maximum
  // allowed number of failures, but a second URL is available.

  // Add a second URL; update with this URL attempted and failed enough times to
  // disqualify the current (first) URL.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.num_checks = 10;
  ucs_data_->update_state.download_urls.emplace_back(
      "http://another/fake/url/");
  Time t = fake_clock_->GetWallclockTime() - TimeDelta::FromSeconds(12);
  for (int i = 0; i < 11; i++) {
    ucs_data_->update_state.download_errors.emplace_back(
        0, ErrorCode::kDownloadTransferError, t);
    t += TimeDelta::FromSeconds(1);
  }

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(1, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedWithSecondUrlHardError) {
  // The UpdateCanStart policy returns true; the first URL fails with a hard
  // error, but a second URL is available.

  // Add a second URL; update with this URL attempted and failed in a way that
  // causes it to switch directly to the next URL.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.num_checks = 10;
  ucs_data_->update_state.download_urls.emplace_back(
      "http://another/fake/url/");
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kPayloadHashMismatchError,
      fake_clock_->GetWallclockTime() - TimeDelta::FromSeconds(1));

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(1, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedUrlWrapsAround) {
  // The UpdateCanStart policy returns true; URL search properly wraps around
  // the last one on the list.

  // Add a second URL; update with this URL attempted and failed in a way that
  // causes it to switch directly to the next URL. We must disable backoff in
  // order for it not to interfere.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  ucs_data_->update_state.num_checks = 1;
  ucs_data_->update_state.is_backoff_disabled = true;
  ucs_data_->update_state.download_urls.emplace_back(
      "http://another/fake/url/");
  ucs_data_->update_state.download_errors.emplace_back(
      1,
      ErrorCode::kPayloadHashMismatchError,
      fake_clock_->GetWallclockTime() - TimeDelta::FromSeconds(1));

  // Check that the UpdateCanStart returns true.
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_TRUE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, NotAllowedNoUsableUrls) {
  // The UpdateCanStart policy returns false; there's a single HTTP URL but its
  // use is forbidden by policy.
  //
  // Note: In the case where no usable URLs are found, the policy should not
  // increment the number of failed attempts! Doing so would result in a
  // non-idempotent semantics, and does not fall within the intended purpose of
  // the backoff mechanism anyway.

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns false.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_FALSE(ucs_data_->result.update_can_start);
  EXPECT_EQ(UpdateCannotStartReason::kCannotDownload,
            ucs_data_->result.cannot_start_reason);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedNoUsableUrlsButP2PEnabled) {
  // The UpdateCanStart policy returns true; there's a single HTTP URL but its
  // use is forbidden by policy, however P2P is enabled. The result indicates
  // that no URL can be used.
  //
  // Note: The number of failed attempts should not increase in this case (see
  // above test).

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(
      new bool(true));
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_TRUE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_GT(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedNoUsableUrlsButEnterpriseEnrolled) {
  // The UpdateCanStart policy returns true; there's a single HTTP URL but its
  // use is forbidden by policy, and P2P is unset on the policy, however the
  // device is enterprise-enrolled so P2P is allowed. The result indicates that
  // no URL can be used.
  //
  // Note: The number of failed attempts should not increase in this case (see
  // above test).

  // Override specific device policy attributes.
  fake_state_.device_policy_provider()->var_au_p2p_enabled()->reset(nullptr);
  fake_state_.device_policy_provider()->var_has_owner()->reset(new bool(false));
  fake_state_.device_policy_provider()->var_http_downloads_enabled()->reset(
      new bool(false));

  // Check that the UpdateCanStart returns true.
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromMinutes(10));
  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_TRUE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_GT(0, ucs_data_->result.download_url_idx);
  EXPECT_TRUE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedScatteringSupressedDueToP2P) {
  // The UpdateCanStart policy returns true; scattering should have applied, but
  // P2P download is allowed. Scattering values are nonetheless returned, and so
  // are download URL values, albeit the latter are not allowed to be used.

  fake_state_.device_policy_provider()->var_scatter_factor()->reset(
      new TimeDelta(TimeDelta::FromMinutes(2)));
  fake_state_.updater_provider()->var_p2p_enabled()->reset(new bool(true));

  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(1));
  ucs_data_->update_state.scatter_wait_period = TimeDelta::FromSeconds(35);

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_FALSE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_TRUE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_FALSE(ucs_data_->result.do_increment_failures);
  EXPECT_EQ(TimeDelta::FromSeconds(35), ucs_data_->result.scatter_wait_period);
  EXPECT_EQ(0, ucs_data_->result.scatter_check_threshold);
}

TEST_F(UmUpdateCanStartPolicyTest, AllowedBackoffSupressedDueToP2P) {
  // The UpdateCanStart policy returns true; backoff should have applied, but
  // P2P download is allowed. Backoff values are nonetheless returned, and so
  // are download URL values, albeit the latter are not allowed to be used.

  const Time curr_time = fake_clock_->GetWallclockTime();
  ucs_data_->update_state = GetDefaultUpdateState(TimeDelta::FromSeconds(10));
  ucs_data_->update_state.download_errors_max = 1;
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(8));
  ucs_data_->update_state.download_errors.emplace_back(
      0,
      ErrorCode::kDownloadTransferError,
      curr_time - TimeDelta::FromSeconds(2));
  fake_state_.updater_provider()->var_p2p_enabled()->reset(new bool(true));

  EXPECT_EQ(EvalStatus::kSucceeded, evaluator_->Evaluate());
  EXPECT_TRUE(ucs_data_->result.update_can_start);
  EXPECT_EQ(0, ucs_data_->result.download_url_idx);
  EXPECT_FALSE(ucs_data_->result.download_url_allowed);
  EXPECT_EQ(0, ucs_data_->result.download_url_num_errors);
  EXPECT_TRUE(ucs_data_->result.p2p_downloading_allowed);
  EXPECT_TRUE(ucs_data_->result.p2p_sharing_allowed);
  EXPECT_TRUE(ucs_data_->result.do_increment_failures);
  EXPECT_LT(curr_time, ucs_data_->result.backoff_expiry);
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
