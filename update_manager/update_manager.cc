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

#include "update_engine/update_manager/update_manager.h"

#include <utility>

#include "update_engine/update_manager/state.h"

namespace chromeos_update_manager {

UpdateManager::UpdateManager(base::TimeDelta evaluation_timeout,
                             base::TimeDelta expiration_timeout,
                             State* state)
    : state_(state),
      evaluation_timeout_(evaluation_timeout),
      expiration_timeout_(expiration_timeout) {}

UpdateManager::~UpdateManager() {}

EvalStatus UpdateManager::PolicyRequest2(
    std::unique_ptr<PolicyInterface> policy,
    std::shared_ptr<PolicyDataInterface> data) {
  return base::MakeRefCounted<PolicyEvaluator>(
             state_.get(),
             std::make_unique<EvaluationContext>(evaluation_timeout_),
             std::move(policy),
             std::move(data))
      ->Evaluate();
}

void UpdateManager::PolicyRequest2(std::unique_ptr<PolicyInterface> policy,
                                   std::shared_ptr<PolicyDataInterface> data,
                                   base::Callback<void(EvalStatus)> callback) {
  auto ec = std::make_unique<EvaluationContext>(
      evaluation_timeout_,
      expiration_timeout_,
      std::unique_ptr<base::Callback<void(EvaluationContext*)>>(nullptr));
  base::MakeRefCounted<PolicyEvaluator>(
      state_.get(), std::move(ec), std::move(policy), std::move(data))
      ->ScheduleEvaluation(std::move(callback));
}

std::unique_ptr<UpdateTimeRestrictionsMonitor>
UpdateManager::BuildUpdateTimeRestrictionsMonitorIfNeeded(
    const chromeos_update_engine::InstallPlan& install_plan,
    UpdateTimeRestrictionsMonitor::Delegate* delegate) {
  if (!install_plan.can_download_be_canceled || delegate == nullptr)
    return nullptr;

  return std::make_unique<UpdateTimeRestrictionsMonitor>(
      state_->device_policy_provider(), delegate);
}

}  // namespace chromeos_update_manager
