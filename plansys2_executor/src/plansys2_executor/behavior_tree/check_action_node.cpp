// Copyright 2020 Intelligent Robotics Lab
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

#include <string>
#include <map>
#include <memory>

#include <plansys2_executor/behavior_tree/check_action_node.hpp>

namespace plansys2
{

CheckAction::CheckAction(
  const std::string & xml_tag_name,
  const BT::NodeConfig & conf)
: ActionNodeBase(xml_tag_name, conf)
{
  action_map_ =
    config().blackboard->get<std::shared_ptr<std::map<std::string, ActionExecutionInfo>>>(
    "action_map");
}

BT::NodeStatus
CheckAction::tick()
{
  std::string action;
  getInput("action", action);

  if ((*action_map_).find(action) == (*action_map_).end()) {
    return BT::NodeStatus::RUNNING;  // Not started yet
  }

  if ((*action_map_)[action].action_executor != nullptr &&
    (*action_map_)[action].action_executor->is_finished() &&
    (*action_map_)[action].at_start_effects_applied &&
    (*action_map_)[action].at_end_effects_applied)
  {
    return BT::NodeStatus::SUCCESS;
  } else {
    return BT::NodeStatus::RUNNING;
  }
}

}  // namespace plansys2
