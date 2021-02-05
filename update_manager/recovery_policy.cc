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

#include "update_engine/update_manager/recovery_policy.h"

namespace chromeos_update_manager {

EvalStatus RecoveryPolicy::Evaluate(EvaluationContext* ec,
                                    State* state,
                                    std::string* error,
                                    PolicyDataInterface* data) const {
  const bool* running_in_minios =
      ec->GetValue(state->updater_provider()->var_running_from_minios());
  if (running_in_minios && (*running_in_minios)) {
    LOG(INFO) << "In Recovery Mode, always allow update check.";
    return EvalStatus::kSucceeded;
  }
  return EvalStatus::kContinue;
}

}  // namespace chromeos_update_manager
