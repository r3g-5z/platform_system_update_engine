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

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_CHROMEOS_POLICY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_CHROMEOS_POLICY_H_

#include <memory>
#include <string>

#include <base/time/time.h>

#include "update_engine/update_manager/next_update_check_policy_impl.h"
#include "update_engine/update_manager/policy_utils.h"

namespace chromeos_update_manager {

// Output information from UpdateBackoffAndDownloadUrl.
struct UpdateBackoffAndDownloadUrlResult {
  // Whether the failed attempt count (maintained by the caller) needs to be
  // incremented.
  bool do_increment_failures;
  // The current backoff expiry. Null if backoff is not in effect.
  base::Time backoff_expiry;
  // The new URL index to use and number of download errors associated with it.
  // Significant iff |do_increment_failures| is false and |backoff_expiry| is
  // null. Negative value means no usable URL was found.
  int url_idx;
  int url_num_errors;
};

// Parameters for update scattering, as returned by UpdateScattering.
struct UpdateScatteringResult {
  bool is_scattering;
  base::TimeDelta wait_period;
  int check_threshold;
};

class ChromeOSPolicy : public Policy {
 public:
  ChromeOSPolicy() = default;
  ~ChromeOSPolicy() override = default;

  EvalStatus UpdateCanBeApplied(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      chromeos_update_engine::ErrorCode* result,
      chromeos_update_engine::InstallPlan* install_plan) const override;

  EvalStatus UpdateCanStart(EvaluationContext* ec,
                            State* state,
                            std::string* error,
                            UpdateDownloadParams* result,
                            UpdateState update_state) const override;

 protected:
  // Policy override.
  std::string PolicyName() const override { return "ChromeOSPolicy"; }

 private:
  friend class UmChromeOSPolicyTest;
  FRIEND_TEST(UmChromeOSPolicyTest, UpdateCheckAllowedWaitsForTheTimeout);
  FRIEND_TEST(UmChromeOSPolicyTest, UpdateCheckAllowedWaitsForOOBE);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringNewWaitPeriodApplies);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringPrevWaitPeriodStillApplies);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringNewCountThresholdApplies);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartNotAllowedScatteringPrevCountThresholdStillApplies);
  FRIEND_TEST(UmChromeOSPolicyTest, UpdateCanStartAllowedScatteringSatisfied);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartAllowedInteractivePreventsScattering);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartAllowedP2PDownloadingBlockedDueToNumAttempts);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCanStartAllowedP2PDownloadingBlockedDueToAttemptsPeriod);
  FRIEND_TEST(UmChromeOSPolicyTest,
              UpdateCheckAllowedNextUpdateCheckOutsideDisallowedInterval);

  // Auxiliary constant (zero by default).
  const base::TimeDelta kZeroInterval;

  // A private policy for determining backoff and the download URL to use.
  // Within |update_state|, |backoff_expiry| and |is_backoff_disabled| are used
  // for determining whether backoff is still in effect; if not,
  // |download_errors| is scanned past |failures_last_updated|, and a new
  // download URL from |download_urls| is found and written to |result->url_idx|
  // (-1 means no usable URL exists); |download_errors_max| determines the
  // maximum number of attempts per URL, according to the Omaha response. If an
  // update failure is identified then |result->do_increment_failures| is set to
  // true; if backoff is enabled, a new backoff period is computed (from the
  // time of failure) based on |num_failures|. Otherwise, backoff expiry is
  // nullified, indicating that no backoff is in effect.
  //
  // If backing off but the previous backoff expiry is unchanged, returns
  // |EvalStatus::kAskMeAgainLater|. Otherwise:
  //
  // * If backing off with a new expiry time, then |result->backoff_expiry| is
  //   set to this time.
  //
  // * Else, |result->backoff_expiry| is set to null, indicating that no backoff
  //   is in effect.
  //
  // In any of these cases, returns |EvalStatus::kSucceeded|. If an error
  // occurred, returns |EvalStatus::kFailed|.
  EvalStatus UpdateBackoffAndDownloadUrl(
      EvaluationContext* ec,
      State* state,
      std::string* error,
      UpdateBackoffAndDownloadUrlResult* result,
      const UpdateState& update_state) const;

  // A private policy for checking whether scattering is due. Writes in |result|
  // the decision as to whether or not to scatter; a wallclock-based scatter
  // wait period, which ranges from zero (do not wait) and no greater than the
  // current scatter factor provided by the device policy (if available) or the
  // maximum wait period determined by Omaha; and an update check-based
  // threshold between zero (no threshold) and the maximum number determined by
  // the update engine. Within |update_state|, |scatter_wait_period| should
  // contain the last scattering period returned by this function, or zero if no
  // wait period is known; |scatter_check_threshold| is the last update check
  // threshold, or zero if no such threshold is known. If not scattering, or if
  // any of the scattering values has changed, returns |EvalStatus::kSucceeded|;
  // otherwise, |EvalStatus::kAskMeAgainLater|.
  EvalStatus UpdateScattering(EvaluationContext* ec,
                              State* state,
                              std::string* error,
                              UpdateScatteringResult* result,
                              const UpdateState& update_state) const;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSPolicy);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_CHROMEOS_POLICY_H_
