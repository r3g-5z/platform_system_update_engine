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
#ifndef UPDATE_ENGINE_UPDATE_MANAGER_POLICY_EVALUATOR_H_
#define UPDATE_ENGINE_UPDATE_MANAGER_POLICY_EVALUATOR_H_

#include <memory>
#include <utility>

#include <base/memory/ref_counted.h>

#include "update_engine/update_manager/evaluation_context.h"
#include "update_engine/update_manager/policy.h"
#include "update_engine/update_manager/policy_interface.h"
#include "update_engine/update_manager/state.h"

namespace chromeos_update_manager {

// This class is the main point of entry for evaluating any kind of policy. The
// reason an instance of this class needs to be ref counted is because normally
// we don't want to keep a reference to an object of this class
// around. Specially when we want to evaluate a policy asychronously. Ref
// counting allows us to pass a pointer to this class in repeated callbacks.  To
// make an instance of this class use base::AdoptRef() or base::MakeRefCounted()
// (See base/memory/ref_counted.h).
class PolicyEvaluator : public base::RefCounted<PolicyEvaluator> {
 public:
  PolicyEvaluator(State* state,
                  std::unique_ptr<EvaluationContext> ec,
                  std::unique_ptr<PolicyInterface> policy,
                  std::shared_ptr<PolicyDataInterface> data)
      : state_(state),
        ec_(std::move(ec)),
        policy_(std::move(policy)),
        data_(std::move(data)) {}
  virtual ~PolicyEvaluator() = default;

  PolicyEvaluator(const PolicyEvaluator&) = delete;
  PolicyEvaluator& operator=(const PolicyEvaluator&) = delete;

  // Evaluations the policy given in the ctor using the provided |data_| and
  // returns the result of the evaluation.
  EvalStatus Evaluate();

  // Same as the above function but the asyncronous version. A call to this
  // function returns immediately and an evalution is scheduled in the main
  // message loop. The passed |callback| is called when the policy is evaluated.
  void ScheduleEvaluation(base::Callback<void(EvalStatus)> callback);

 private:
  friend class base::RefCounted<PolicyEvaluator>;

  // Internal function to reschedule policy evaluation.
  void OnPolicyReadyToEvaluate(base::Callback<void(EvalStatus)> callback);
  State* state_;
  std::unique_ptr<EvaluationContext> ec_;
  std::unique_ptr<PolicyInterface> policy_;
  std::shared_ptr<PolicyDataInterface> data_;
};

}  // namespace chromeos_update_manager

#endif  // UPDATE_ENGINE_UPDATE_MANAGER_POLICY_EVALUATOR_H_
