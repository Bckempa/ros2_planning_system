// Copyright 2019 Intelligent Robotics Lab
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLANSYS2_PLANNER__PLANNERINTERFACE_HPP_
#define PLANSYS2_PLANNER__PLANNERINTERFACE_HPP_

#include <optional>
#include <string>

#include <plansys2_msgs/Plan.h>

namespace plansys2
{

class PlannerInterface
{
public:
  PlannerInterface() {}

  virtual std::optional<plansys2_msgs::Plan> getPlan(
    const std::string & domain, const std::string & problem,
    const std::string & node_namespace) = 0;
};

}  // namespace plansys2

#endif  // PLANSYS2_PLANNER__PLANNERINTERFACE_HPP_
