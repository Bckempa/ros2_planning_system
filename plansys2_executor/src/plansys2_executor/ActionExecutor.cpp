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

#include <string>
#include <memory>
#include <vector>

#include <plansys2_pddl_parser/Utils.h>

#include <plansys2_executor/ActionExecutor.hpp>

namespace plansys2
{

using std::placeholders::_1;
using namespace std::chrono_literals;

ActionExecutor::ActionExecutor(
  const std::string & action,
  std::shared_ptr<ros::lifecycle::ManagedNode> node)
: node_(node), state_(IDLE), completion_(0.0)
{

  action_hub_pub_.reset(new ros::lifecycle::LifecyclePublisher<plansys2_msgs::ActionExecution>(node_,
											       "/actions_hub"));
  action_hub_sub_.reset(new ros::lifecycle::LifecycleSubscriber_<plansys2_msgs::ActionExecution>(node_,
												 "/actions_hub",
												 &ActionExecutor::action_hub_callback,
												 this));

  state_time_ = ros::Time::now();

  action_ = action;
  action_name_ = get_name(action);
  action_params_ = get_params(action);

  start_execution_ = ros::Time::now();
  state_time_ = start_execution_;
}

void ActionExecutor::action_hub_callback(const plansys2_msgs::ActionExecution::ConstPtr &msg)
{
  last_msg = *msg;

  switch (msg->type) {
    case plansys2_msgs::ActionExecution::REQUEST:
    case plansys2_msgs::ActionExecution::CONFIRM:
    case plansys2_msgs::ActionExecution::REJECT:
    case plansys2_msgs::ActionExecution::CANCEL:
      // These cases have no meaning requester
      break;
    case plansys2_msgs::ActionExecution::RESPONSE:
      if (msg->arguments == action_params_ && msg->action == action_name_) {
        if (state_ == DEALING) {
          current_performer_id_ = msg->node_id;
          state_ = RUNNING;
          if (waiting_timer_ != nullptr)
            waiting_timer_->stop();
          waiting_timer_ = nullptr;
          start_execution_ = ros::Time::now();
          state_time_ = ros::Time::now();
          confirm_performer(msg->node_id);
        } else {
          reject_performer(msg->node_id);
        }
      }
      break;
    case plansys2_msgs::ActionExecution::FEEDBACK:
      if (state_ != RUNNING || msg->arguments != action_params_ || msg->action != action_name_ ||
        msg->node_id != current_performer_id_)
      {
        return;
      }
      feedback_ = msg->status;
      completion_ = msg->completion;
      state_time_ = ros::Time::now();

      break;
    case plansys2_msgs::ActionExecution::FINISH:
      if (msg->arguments == action_params_ &&
        msg->action == action_name_ && msg->node_id == current_performer_id_)
      {
        if (msg->success) {
          state_ = SUCCESS;
        } else {
          state_ = FAILURE;
        }

        feedback_ = msg->status;
        completion_ = msg->completion;

        state_time_ = ros::Time::now();

        action_hub_pub_->on_deactivate();
        action_hub_pub_ = nullptr;
	action_hub_sub_ = nullptr;
      }
      break;
    default:
      ROS_ERROR("%s -- Msg %d type not recognized in %s executor requester",
		getName().c_str(),
		msg->type, action_.c_str());
      break;
  }
}

void
ActionExecutor::confirm_performer(const std::string & node_id)
{
  plansys2_msgs::ActionExecution msg;
  msg.type = plansys2_msgs::ActionExecution::CONFIRM;
  msg.node_id = node_id;
  msg.action = action_name_;
  msg.arguments = action_params_;

  action_hub_pub_->publish(msg);
}

void
ActionExecutor::reject_performer(const std::string & node_id)
{
  plansys2_msgs::ActionExecution msg;
  msg.type = plansys2_msgs::ActionExecution::REJECT;
  msg.node_id = node_id;
  msg.action = action_name_;
  msg.arguments = action_params_;

  action_hub_pub_->publish(msg);
}

void
ActionExecutor::request_for_performers()
{
  plansys2_msgs::ActionExecution msg;
  msg.type = plansys2_msgs::ActionExecution::REQUEST;
  msg.node_id = node_->get_name();
  msg.action = action_name_;
  msg.arguments = action_params_;

  action_hub_pub_->publish(msg);
}

BT::NodeStatus
ActionExecutor::get_status()
{
  switch (state_) {
    case IDLE:
      return BT::NodeStatus::IDLE;
      break;
    case DEALING:
    case RUNNING:
      return BT::NodeStatus::RUNNING;
      break;
    case SUCCESS:
      return BT::NodeStatus::SUCCESS;
      break;
    case FAILURE:
      return BT::NodeStatus::FAILURE;
      break;
    default:
      return BT::NodeStatus::IDLE;
      break;
  }
}

bool
ActionExecutor::is_finished()
{
  return state_ == SUCCESS || state_ == FAILURE;
}

BT::NodeStatus
ActionExecutor::tick(const ros::Time & now)
{
  switch (state_) {
    case IDLE:
      state_ = DEALING;
      state_time_ = ros::Time::now();

      action_hub_pub_->on_activate();

      completion_ = 0.0;
      feedback_ = "";

      request_for_performers();
      waiting_timer_ = std::make_shared<ros::WallTimer>(node_->getBaseNode().createWallTimer(ros::WallDuration(1.0),
											     &ActionExecutor::wait_timeout, this));
      break;
    case DEALING:
      {
        auto time_since_dealing = (ros::Time::now() - state_time_).toSec();
        if (time_since_dealing > 30.0) {
          ROS_ERROR("%s -- Aborting %s. Timeout after requesting for 30 seconds",
		    getName().c_str(),
		    action_.c_str());
          state_ = FAILURE;
        }
      }
      break;

    case RUNNING:
      break;
    case SUCCESS:
    case FAILURE:
    case CANCELLED:
      break;
    default:
      break;
  }

  return get_status();
}

void
ActionExecutor::cancel()
{
  state_ = CANCELLED;
  plansys2_msgs::ActionExecution msg;
  msg.type = plansys2_msgs::ActionExecution::CANCEL;
  msg.node_id = current_performer_id_;
  msg.action = action_name_;
  msg.arguments = action_params_;

  action_hub_pub_->publish(msg);
}

std::string
ActionExecutor::get_name(const std::string & action_expr)
{
  std::string working_action_expr = parser::pddl::getReducedString(action_expr);
  working_action_expr.erase(0, 1);  // remove initial (
  working_action_expr.pop_back();  // remove last )

  size_t delim = working_action_expr.find(" ");

  return working_action_expr.substr(0, delim);
}

std::vector<std::string>
ActionExecutor::get_params(const std::string & action_expr)
{
  std::vector<std::string> ret;

  std::string working_action_expr = parser::pddl::getReducedString(action_expr);
  working_action_expr.erase(0, 1);  // remove initial (
  working_action_expr.pop_back();  // remove last )

  size_t delim = working_action_expr.find(" ");

  working_action_expr = working_action_expr.substr(delim + 1);

  size_t start = 0, end = 0;
  while (end != std::string::npos) {
    end = working_action_expr.find(" ", start);
    auto param = working_action_expr.substr(
      start, (end == std::string::npos) ? std::string::npos : end - start);
    ret.push_back(param);
    start = ((end > (std::string::npos - 1)) ? std::string::npos : end + 1);
  }

  return ret;
}

void
ActionExecutor::wait_timeout(const ros::WallTimerEvent &event)
{
  if (state_ == RUNNING) {
    ROS_WARN_STREAM("This timer wasn't properly cancelled");
    if (waiting_timer_!= nullptr) {
      waiting_timer_->stop();
    }
  } else {
    ROS_WARN("%s -- No action performer for %s. retrying", getName().c_str(), action_.c_str());
    request_for_performers();
  }
}

}  // namespace plansys2
