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

#ifndef PLANSYS2_BT_ACTIONS__BTACTION_HPP_
#define PLANSYS2_BT_ACTIONS__BTACTION_HPP_

#include <memory>
#include <string>
#include <vector>

#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/xml_parsing.h>
#include <behaviortree_cpp/loggers/bt_file_logger_v2.h>
#include <behaviortree_cpp/loggers/bt_minitrace_logger.h>

#ifdef ZMQ_FOUND
#include <behaviortree_cpp/loggers/bt_zmq_publisher.h>
#endif

#include <plansys2_executor/ActionExecutorClient.hpp>
#include <ros/ros.h>

namespace plansys2
{

class BTAction : public plansys2::ActionExecutorClient
{
public:
  explicit BTAction(ros::NodeHandle nh,
    const std::string & action,
    const std::chrono::nanoseconds & rate);

  const std::string & getActionName() const {return action_;}
  const std::string & getBTFile() const {return bt_xml_file_;}

protected:

  bool onConfigure();

  bool onCleanup();

  bool onActivate();

  bool onDeactivate();

  void do_work();

  BT::BehaviorTreeFactory factory_;

private:
  BT::Tree tree_;
  BT::Blackboard::Ptr blackboard_;
  std::string action_;
  std::string bt_xml_file_;
  std::vector<std::string> plugin_list_;
  bool finished_;
  #ifdef ZMQ_FOUND
  std::unique_ptr<BT::PublisherZMQ> publisher_zmq_;
  #endif
  std::unique_ptr<BT::FileLogger2> bt_file_logger_;
  std::unique_ptr<BT::MinitraceLogger> bt_minitrace_logger_;
};

}  // namespace plansys2

#endif  // PLANSYS2_BT_ACTIONS__BTACTION_HPP_
