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

#include "update_engine/update_manager/omaha_request_params_policy.h"

#include <string>

#include "update_engine/common/constants.h"
#include "update_engine/common/system_state.h"
#include "update_engine/cros/omaha_request_params.h"

using chromeos_update_engine::SystemState;
using std::string;

namespace chromeos_update_manager {

namespace {
constexpr char kMarketSegmentConsumer[] = "consumer";
}  // namespace

EvalStatus OmahaRequestParamsPolicy::Evaluate(EvaluationContext* ec,
                                              State* state,
                                              string* error,
                                              PolicyDataInterface* data) const {
  auto request_params = SystemState::Get()->request_params();

  const bool* market_segment_disabled_p =
      ec->GetValue(state->updater_provider()->var_market_segment_disabled());
  if (market_segment_disabled_p == nullptr || !(*market_segment_disabled_p)) {
    request_params->set_market_segment(kMarketSegmentConsumer);
  }

  // If no device policy was loaded, nothing else to do.
  DevicePolicyProvider* const dp_provider = state->device_policy_provider();
  const bool* device_policy_is_loaded_p =
      ec->GetValue(dp_provider->var_device_policy_is_loaded());
  if (!device_policy_is_loaded_p || !(*device_policy_is_loaded_p)) {
    return EvalStatus::kContinue;
  }

  if (market_segment_disabled_p == nullptr || !(*market_segment_disabled_p)) {
    const string* market_segment_p =
        ec->GetValue(dp_provider->var_market_segment());
    if (market_segment_p) {
      request_params->set_market_segment(*market_segment_p);
    }
  }

  return EvalStatus::kSucceeded;
}

}  // namespace chromeos_update_manager
