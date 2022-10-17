//
// Copyright (C) 2022 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_CROS_HIBERNATE_CHROMEOS_H_
#define UPDATE_ENGINE_CROS_HIBERNATE_CHROMEOS_H_

#include <memory>

#include "update_engine/common/hibernate_interface.h"

namespace chromeos_update_engine {

// The Chrome OS implementation of the HibernateInterface. This interface
// provides information about the state of hibernate and resume.
class HibernateChromeOS : public HibernateInterface {
 public:
  HibernateChromeOS() = default;
  HibernateChromeOS(const HibernateChromeOS&) = delete;
  HibernateChromeOS& operator=(const HibernateChromeOS&) = delete;

  ~HibernateChromeOS() override = default;

  // HibernateInterface overrides.

  // Returns true if the system is resuming from hibernate.
  bool IsResuming() override;

 private:
  bool not_resuming_from_hibernate_ = false;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CROS_HIBERNATE_CHROMEOS_H_
