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

#ifndef UPDATE_ENGINE_UPDATE_MANAGER_MOCK_POLICY_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_MOCK_POLICY_H_

#include <string>

#include <gmock/gmock.h>

#include "update_engine/update_manager/default_policy.h"
#include "update_engine/update_manager/policy.h"

namespace chromeos_update_manager {

// A mocked implementation of Policy.
class MockPolicy : public Policy {
 public:
  MockPolicy() {
    // We defer to the corresponding DefaultPolicy methods, by default.
    ON_CALL(*this,
            UpdateCanStart(
                testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillByDefault(
            testing::Invoke(&default_policy_, &DefaultPolicy::UpdateCanStart));
  }
  ~MockPolicy() override {}

  MOCK_CONST_METHOD5(UpdateCanStart,
                     EvalStatus(EvaluationContext*,
                                State*,
                                std::string*,
                                UpdateDownloadParams*,
                                UpdateState));

  MOCK_CONST_METHOD4(
      UpdateDownloadAllowed,
      EvalStatus(EvaluationContext*, State*, std::string*, bool*));

 protected:
  // Policy override.
  std::string PolicyName() const override { return "MockPolicy"; }

 private:
  DefaultPolicy default_policy_;

  DISALLOW_COPY_AND_ASSIGN(MockPolicy);
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_MOCK_POLICY_H_
