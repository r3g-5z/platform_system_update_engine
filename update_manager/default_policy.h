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

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_

#include <memory>
#include <string>

#include <base/time/time.h>

#include "update_engine/update_manager/policy.h"

namespace chromeos_update_manager {

// The DefaultPolicy is a safe Policy implementation that doesn't fail. The
// values returned by this policy are safe default in case of failure of the
// actual policy being used by the UpdateManager.
class DefaultPolicy : public Policy {
 public:
  DefaultPolicy() = default;
  ~DefaultPolicy() override = default;

  EvalStatus UpdateCanStart(EvaluationContext* ec,
                            State* state,
                            std::string* error,
                            UpdateDownloadParams* result,
                            UpdateState update_state) const override;

 protected:
  // Policy override.
  std::string PolicyName() const override { return "DefaultPolicy"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPolicy);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_DEFAULT_POLICY_H_
