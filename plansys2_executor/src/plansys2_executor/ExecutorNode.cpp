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

#include <filesystem>

#include <algorithm>
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>

#include <plansys2_executor/ExecutorNode.hpp>
#include <plansys2_executor/ActionExecutor.hpp>
#include <plansys2_executor/BTBuilder.hpp>
#include <plansys2_problem_expert/Utils.hpp>
#include <plansys2_pddl_parser/Utils.h>

//#include <lifecycle_msgs/state.hpp>
#include <plansys2_msgs/ActionExecutionInfo.h>
#include <plansys2_msgs/Plan.h>

#include <ros/package.h>

#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/utils/shared_library.h>
#include <behaviortree_cpp/blackboard.h>

#include <plansys2_executor/behavior_tree/execute_action_node.hpp>
#include <plansys2_executor/behavior_tree/wait_action_node.hpp>
#include <plansys2_executor/behavior_tree/check_action_node.hpp>
#include <plansys2_executor/behavior_tree/wait_atstart_req_node.hpp>
#include <plansys2_executor/behavior_tree/check_overall_req_node.hpp>
#include <plansys2_executor/behavior_tree/check_atend_req_node.hpp>
#include <plansys2_executor/behavior_tree/check_timeout_node.hpp>
#include <plansys2_executor/behavior_tree/apply_atstart_effect_node.hpp>
#include <plansys2_executor/behavior_tree/apply_atend_effect_node.hpp>

namespace plansys2
{

using namespace std::chrono_literals;

  ExecutorNode::ExecutorNode(ros::NodeHandle _nh)
  : ros::lifecycle::ManagedNode(_nh),
  bt_builder_loader_("plansys2_executor", "plansys2::BTBuilder")
{

  // In ROS1 we don't have to declareparameters
  /*
  this->declare_parameter<std::string>("default_action_bt_xml_filename", "");
  this->declare_parameter<std::string>("default_start_action_bt_xml_filename", "");
  this->declare_parameter<std::string>("default_end_action_bt_xml_filename", "");
  this->declare_parameter<std::string>("bt_builder_plugin", "");
  this->declare_parameter<int>("action_time_precision", 3);
  this->declare_parameter<bool>("enable_dotgraph_legend", true);
  this->declare_parameter<bool>("print_graph", false);
  this->declare_parameter("action_timeouts.actions", std::vector<std::string>{});
  // Declaring individual action parameters so they can be queried on the command line
  auto action_timeouts_actions = this->get_parameter("action_timeouts.actions").as_string_array();
  for (auto action : action_timeouts_actions) {
    this->declare_parameter<double>(
      "action_timeouts." + action + ".duration_overrun_percentage",
      0.0);
      }*/
  /*
#ifdef ZMQ_FOUND
  this->declare_parameter<bool>("enable_groot_monitoring", true);
  this->declare_parameter<int>("publisher_port", 2666);
  this->declare_parameter<int>("server_port", 2667);
  this->declare_parameter<int>("max_msgs_per_second", 25);
#endif
  */
  execute_plan_action_server_.reset( new actionlib::SimpleActionServer<plansys2_msgs::ExecutePlanAction>(getBaseNode(),"execute_plan", false));
  
  execute_plan_action_server_->registerGoalCallback(boost::bind(&ExecutorNode::handle_accepted, this));
  execute_plan_action_server_->registerPreemptCallback(boost::bind(&ExecutorNode::handle_cancel, this));
													

  get_ordered_sub_goals_service_ = getBaseNode().advertiseService("executor/get_ordered_sub_goals",
								  &ExecutorNode::get_ordered_sub_goals_service_callback,
								  this);

  get_plan_service_ = getBaseNode().advertiseService("executor/get_plan",
						     &ExecutorNode::get_plan_service_callback,
						     this);
}


bool
ExecutorNode::onConfigure()
{
  ROS_INFO("[%s] Configuring...", get_name().c_str());

  std::string default_action_bt_xml_filename;
  getBaseNode().getParam("default_action_bt_xml_filename", default_action_bt_xml_filename);
  
  if (default_action_bt_xml_filename.empty()) {
    default_action_bt_xml_filename =
      ros::package::getPath("plansys2_executor") +
      "/behavior_trees/plansys2_action_bt.xml";
  }

  std::ifstream action_bt_ifs(default_action_bt_xml_filename);
  if (!action_bt_ifs) {
    ROS_ERROR_STREAM(get_name().c_str() << " Error openning [" << default_action_bt_xml_filename << "]");
    return false;
  }

  action_bt_xml_.assign(
    std::istreambuf_iterator<char>(action_bt_ifs), std::istreambuf_iterator<char>());

  std::string default_start_action_bt_xml_filename;
  getBaseNode().getParam("default_start_action_bt_xml_filename", default_start_action_bt_xml_filename);
  
  if (default_start_action_bt_xml_filename.empty()) {
    default_start_action_bt_xml_filename =
      ros::package::getPath("plansys2_executor") +
      "/behavior_trees/plansys2_start_action_bt.xml";
  }

  std::ifstream start_action_bt_ifs(default_start_action_bt_xml_filename);
  if (!start_action_bt_ifs) {
    ROS_ERROR_STREAM(get_name().c_str() <<
		     "Error openning [" << default_start_action_bt_xml_filename << "]");
    return false;
  }

  start_action_bt_xml_.assign(
    std::istreambuf_iterator<char>(start_action_bt_ifs), std::istreambuf_iterator<char>());

  std::string default_end_action_bt_xml_filename;
  getBaseNode().getParam("default_end_action_bt_xml_filename", default_end_action_bt_xml_filename);
  if (default_end_action_bt_xml_filename.empty()) {
    default_end_action_bt_xml_filename =
      ros::package::getPath("plansys2_executor") +
      "/behavior_trees/plansys2_end_action_bt.xml";
  }

  std::ifstream end_action_bt_ifs(default_end_action_bt_xml_filename);
  if (!end_action_bt_ifs) {
    ROS_ERROR_STREAM(get_name().c_str() <<
		     "Error openning [" << default_end_action_bt_xml_filename << "]");
    return false;
  }

  end_action_bt_xml_.assign(std::istreambuf_iterator<char>(end_action_bt_ifs), std::istreambuf_iterator<char>());

  dotgraph_pub_.reset( new ros::lifecycle::LifecyclePublisher<std_msgs::String>(shared_from_this(), "dot_graph"));
  execution_info_pub_.reset( new ros::lifecycle::LifecyclePublisher<plansys2_msgs::ActionExecutionInfo>(shared_from_this(), "action_execution_info"));
  executing_plan_pub_.reset( new ros::lifecycle::LifecyclePublisher<plansys2_msgs::Plan>(shared_from_this(), "executing_plan"));

  domain_client_ = std::make_shared<plansys2::DomainExpertClient>();
  problem_client_ = std::make_shared<plansys2::ProblemExpertClient>();
  planner_client_ = std::make_shared<plansys2::PlannerClient>();

  // Start action server
  execute_plan_action_server_->start();
  
  ROS_INFO("[%s] Configured", get_name().c_str());
  return true;
}

bool
ExecutorNode::onActivate()
{
  ROS_INFO("[%s] Activating...", get_name().c_str());
  dotgraph_pub_->on_activate();
  execution_info_pub_->on_activate();
  executing_plan_pub_->on_activate();
  ROS_INFO("[%s] Activated", get_name().c_str());

  return true;
}

bool
ExecutorNode::onDeactivate()
{
  ROS_INFO("[%s] Deactivating...", get_name().c_str());
  dotgraph_pub_->on_deactivate();
  executing_plan_pub_->on_deactivate();
  ROS_INFO("[%s] Deactivated", get_name().c_str());

  return true;
}

bool
ExecutorNode::onCleanup()
{
  ROS_INFO("[%s] Cleaning up...", get_name().c_str());
  dotgraph_pub_.reset();
  executing_plan_pub_.reset();
  ROS_INFO("[%s] Cleaned up", get_name().c_str());

  return true;
}

bool
ExecutorNode::onShutdown()
{
  ROS_INFO("[%s] Shutting down...", get_name().c_str());
  dotgraph_pub_.reset();
  executing_plan_pub_.reset();
  ROS_INFO("[%s] Shutted down", get_name().c_str());

  return true;
}

bool
ExecutorNode::onError(std::exception &)
{
  ROS_ERROR("[%s] Error transition", get_name().c_str());

  return true;
}

bool
ExecutorNode::get_ordered_sub_goals_service_callback(plansys2_msgs::GetOrderedSubGoals::Request &request,
						     plansys2_msgs::GetOrderedSubGoals::Response &response)
{
  if (ordered_sub_goals_.has_value()) {
    response.sub_goals = ordered_sub_goals_.value();
    response.success = true;
  } else {
    response.success = false;
    response.error_info = "No current plan.";
  }

  return true;
}

std::optional<std::vector<plansys2_msgs::Tree>>
ExecutorNode::getOrderedSubGoals()
{
  if (!current_plan_.has_value()) {
    return {};
  }

  auto goal = problem_client_->getGoal();
  auto local_predicates = problem_client_->getPredicates();
  auto local_functions = problem_client_->getFunctions();

  std::vector<plansys2_msgs::Tree> ordered_goals;
  std::vector<uint32_t> unordered_subgoals = parser::pddl::getSubtreeIds(goal);

  // just in case some goals are already satisfied
  for (auto it = unordered_subgoals.begin(); it != unordered_subgoals.end(); ) {
    if (check(goal, local_predicates, local_functions, *it)) {
      plansys2_msgs::Tree new_goal;
      parser::pddl::fromString(new_goal, "(and " + parser::pddl::toString(goal, (*it)) + ")");
      ordered_goals.push_back(new_goal);
      it = unordered_subgoals.erase(it);
    } else {
      ++it;
    }
  }

  for (const auto & plan_item : current_plan_.value().items) {
    std::shared_ptr<plansys2_msgs::DurativeAction> action =
      domain_client_->getDurativeAction(
      get_action_name(plan_item.action), get_action_params(plan_item.action));
    apply(action->at_start_effects, local_predicates, local_functions);
    apply(action->at_end_effects, local_predicates, local_functions);

    for (auto it = unordered_subgoals.begin(); it != unordered_subgoals.end(); ) {
      if (check(goal, local_predicates, local_functions, *it)) {
        plansys2_msgs::Tree new_goal;
        parser::pddl::fromString(new_goal, "(and " + parser::pddl::toString(goal, (*it)) + ")");
        ordered_goals.push_back(new_goal);
        it = unordered_subgoals.erase(it);
      } else {
        ++it;
      }
    }
  }

  return ordered_goals;
}

bool
ExecutorNode::get_plan_service_callback(plansys2_msgs::GetPlan::Request &request,
					plansys2_msgs::GetPlan::Response &response)
{
  if (current_plan_) {
    response.success = true;
    response.plan = current_plan_.value();
  } else {
    response.success = false;
    response.error_info = "Plan not available";
  }

  return true;
}


void ExecutorNode::handle_goal()
{
  ROS_DEBUG("[%s] Received goal request with order", get_name().c_str());

  current_plan_ = {};
  ordered_sub_goals_ = {};

  return;
}

void
ExecutorNode::handle_cancel()
{
  ROS_DEBUG("%s -- Received request to cancel goal", get_name().c_str());

  cancel_plan_requested_ = true;

  return;
}

void ExecutorNode::execute(plansys2_msgs::ExecutePlanGoalConstPtr _goal)
{
  plansys2_msgs::ExecutePlanFeedback feedback;
  plansys2_msgs::ExecutePlanResult result;

  cancel_plan_requested_ = false;

  current_plan_ = _goal->plan;

  if (!current_plan_.has_value()) {
    ROS_ERROR("%s -- No plan found", get_name().c_str());
    result.success = false;
    execute_plan_action_server_->setSucceeded(result);

    // Publish void plan
    executing_plan_pub_->publish(plansys2_msgs::Plan());
    return;
  }
  
  executing_plan_pub_->publish(current_plan_.value());

  auto action_map = std::make_shared<std::map<std::string, ActionExecutionInfo>>();
  std::vector<std::string> action_timeouts_actions;

  getBaseNode().getParam("action_timeouts/actions", action_timeouts_actions);

  for (const auto & plan_item : current_plan_.value().items) {
    auto index = BTBuilder::to_action_id(plan_item, 3);

    (*action_map)[index] = ActionExecutionInfo();
    (*action_map)[index].action_executor =
      ActionExecutor::make_shared(plan_item.action, shared_from_this());
    (*action_map)[index].durative_action_info =
      domain_client_->getDurativeAction(
      get_action_name(plan_item.action), get_action_params(plan_item.action));

    (*action_map)[index].duration = plan_item.duration;
    std::string action_name = (*action_map)[index].durative_action_info->name;
    double dop;

    if (std::find(
        action_timeouts_actions.begin(), action_timeouts_actions.end(),
        action_name) != action_timeouts_actions.end() &&
	getBaseNode().getParam("action_timeouts/" + action_name + "/duration_overrun_percentage", dop) )
    {
      (*action_map)[index].duration_overrun_percentage = dop;
    }
    ROS_INFO("%s -- Action %s timeout percentage %f",
	     get_name().c_str(),
	     action_name.c_str(),
      (*action_map)[index].duration_overrun_percentage);
  }

  ordered_sub_goals_ = getOrderedSubGoals();

  std::string bt_builder_plugin;
  getBaseNode().getParam("bt_builder_plugin", bt_builder_plugin);
  if (bt_builder_plugin.empty()) {
    bt_builder_plugin = "SimpleBTBuilder";
  }

  //std::shared_ptr<plansys2::BTBuilder> bt_builder;
  boost::shared_ptr<plansys2::BTBuilder> bt_builder;
  try {
    //bt_builder = bt_builder_loader_.createSharedInstance("plansys2::" + bt_builder_plugin);
    bt_builder = bt_builder_loader_.createInstance("plansys2::" + bt_builder_plugin);

  } catch (pluginlib::PluginlibException & ex) {
    ROS_ERROR("%s -- pluginlib error: %s", get_name().c_str(), ex.what());
  }

  if (bt_builder_plugin == "SimpleBTBuilder") {
    bt_builder->initialize(action_bt_xml_);
  } else if (bt_builder_plugin == "STNBTBuilder") {
    int precision;
    getBaseNode().getParam("action_time_precision", precision);
    bt_builder->initialize(start_action_bt_xml_, end_action_bt_xml_, precision);
  }
  auto blackboard = BT::Blackboard::create();

  blackboard->set("action_map", action_map);
  blackboard->set("node", shared_from_this());
  blackboard->set("domain_client", domain_client_);
  blackboard->set("problem_client", problem_client_);

  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<ExecuteAction>("ExecuteAction");
  factory.registerNodeType<WaitAction>("WaitAction");
  factory.registerNodeType<CheckAction>("CheckAction");
  factory.registerNodeType<CheckOverAllReq>("CheckOverAllReq");
  factory.registerNodeType<WaitAtStartReq>("WaitAtStartReq");
  factory.registerNodeType<CheckAtEndReq>("CheckAtEndReq");
  factory.registerNodeType<ApplyAtStartEffect>("ApplyAtStartEffect");
  factory.registerNodeType<ApplyAtEndEffect>("ApplyAtEndEffect");
  factory.registerNodeType<CheckTimeout>("CheckTimeout");


  auto bt_xml_tree = bt_builder->get_tree(current_plan_.value());
  std_msgs::String dotgraph_msg;
  bool enable_dot = false;
  getBaseNode().getParam("enable_dotgraph_legend", enable_dot);
  bool print_graph = false;
  getBaseNode().getParam("print_graph", print_graph);
  dotgraph_msg.data = bt_builder->get_dotgraph(
    action_map, enable_dot, print_graph);
  dotgraph_pub_->publish(dotgraph_msg);

  std::filesystem::path tp = std::filesystem::temp_directory_path();
  std::ofstream out(std::string("/tmp/") + get_namespace() + "/bt.xml");
  out << bt_xml_tree;
  out.close();

  auto tree = factory.createTreeFromText(bt_xml_tree, blackboard);

#ifdef ZMQ_FOUND
  /*
// ANA HACK -- FIRST LET'S CHECK STUFF WORKS WITHOUT ZMQ
  unsigned int publisher_port = getBaseNode().getParam("publisher_port").as_int();
  unsigned int server_port = getBaseNode().getParam("server_port").as_int();
  unsigned int max_msgs_per_second = getBaseNode().getParam("max_msgs_per_second").as_int();

  std::unique_ptr<BT::PublisherZMQ> publisher_zmq;
  if (getBaseNode().getParam("enable_groot_monitoring").as_bool()) {
    ROS_DEBUG(
      get_logger(),
      "[%s] Groot monitoring: Publisher port: %d, Server port: %d, Max msgs per second: %d",
      get_name(), publisher_port, server_port, max_msgs_per_second);
    try {
      publisher_zmq.reset(
        new BT::PublisherZMQ(
          tree, max_msgs_per_second, publisher_port,
          server_port));
    } catch (const BT::LogicError & exc) {
      ROS_ERROR(get_logger(), "ZMQ error: %s", exc.what());
    }
    }*/
#endif

  auto info_pub = getBaseNode().createWallTimer(ros::WallDuration(1.0),						
						[this, &action_map](const ros::WallTimerEvent &event) {
						  auto msgs = get_feedback_info(action_map);
						  for (const auto & msg : msgs) {
						    execution_info_pub_->publish(msg);
						  }
						});

  ros::Rate rate(10);
  auto status = BT::NodeStatus::RUNNING;

  while (status == BT::NodeStatus::RUNNING && !cancel_plan_requested_) {
    try {
      status = tree.tickOnce();
    } catch (std::exception & e) {
      std::cerr << e.what() << std::endl;
      status == BT::NodeStatus::FAILURE;
    }

    feedback.action_execution_status = get_feedback_info(action_map);
    execute_plan_action_server_->publishFeedback(feedback);
    
    dotgraph_msg.data = bt_builder->get_dotgraph(action_map, enable_dot);
    dotgraph_pub_->publish(dotgraph_msg);

    rate.sleep();
  }

  if (cancel_plan_requested_) {
    tree.haltTree();
  }

  if (status == BT::NodeStatus::FAILURE) {
    tree.haltTree();
    ROS_ERROR("%s -- Executor BT finished with FAILURE state",
	      get_name().c_str());
  }

  dotgraph_msg.data = bt_builder->get_dotgraph(action_map, enable_dot);
  dotgraph_pub_->publish(dotgraph_msg);

  result.success = status == BT::NodeStatus::SUCCESS;
  result.action_execution_status = get_feedback_info(action_map);

  size_t i = 0;
  while (i < result.action_execution_status.size() && result.success) {
    if (result.action_execution_status[i].status !=
      plansys2_msgs::ActionExecutionInfo::SUCCEEDED)
    {
      result.success = false;
    }
    i++;
  }

  if (ros::ok()) {
    execute_plan_action_server_->setSucceeded(result);
    if (result.success) {
      ROS_INFO("%s -- Plan Succeeded", get_name().c_str());
    } else {
      ROS_INFO("%s -- Plan Failed", get_name().c_str());
    }
  }
}

void ExecutorNode::handle_accepted()
{
  handle_goal();
  // if (not busy?)
  goal_ = execute_plan_action_server_->acceptNewGoal();

  using namespace std::placeholders;
  std::thread th( &ExecutorNode::execute, this, goal_ );
  th.detach();
}

std::vector<plansys2_msgs::ActionExecutionInfo>
ExecutorNode::get_feedback_info(
  std::shared_ptr<std::map<std::string,
  ActionExecutionInfo>> action_map)
{
  std::vector<plansys2_msgs::ActionExecutionInfo> ret;

  if (!action_map) {
    return ret;
  }

  for (const auto & action : *action_map) {
    if (!action.second.action_executor) {
      ROS_WARN("%s -- Action executor does not exist for %s. Skipping", get_name().c_str(),
	       action.first.c_str());
      continue;
    }

    plansys2_msgs::ActionExecutionInfo info;
    switch (action.second.action_executor->get_internal_status()) {
      case ActionExecutor::IDLE:
      case ActionExecutor::DEALING:
        info.status = plansys2_msgs::ActionExecutionInfo::NOT_EXECUTED;
        break;
      case ActionExecutor::RUNNING:
        info.status = plansys2_msgs::ActionExecutionInfo::EXECUTING;
        break;
      case ActionExecutor::SUCCESS:
        info.status = plansys2_msgs::ActionExecutionInfo::SUCCEEDED;
        break;
      case ActionExecutor::FAILURE:
        info.status = plansys2_msgs::ActionExecutionInfo::FAILED;
        break;
      case ActionExecutor::CANCELLED:
        info.status = plansys2_msgs::ActionExecutionInfo::CANCELLED;
        break;
    }

    info.action_full_name = action.first;

    info.start_stamp = action.second.action_executor->get_start_time();
    info.status_stamp = action.second.action_executor->get_status_time();
    info.action = action.second.action_executor->get_action_name();

    info.arguments = action.second.action_executor->get_action_params();
    info.duration = ros::Duration(action.second.duration);
    info.completion = action.second.action_executor->get_completion();
    info.message_status = action.second.action_executor->get_feedback();

    ret.push_back(info);
  }

  return ret;
}

void
ExecutorNode::print_execution_info(
  std::shared_ptr<std::map<std::string, ActionExecutionInfo>> exec_info)
{
  fprintf(stderr, "Execution info =====================\n");

  for (const auto & action_info : *exec_info) {
    fprintf(stderr, "[%s]", action_info.first.c_str());
    switch (action_info.second.action_executor->get_internal_status()) {
      case ActionExecutor::IDLE:
        fprintf(stderr, "\tIDLE\n");
        break;
      case ActionExecutor::DEALING:
        fprintf(stderr, "\tDEALING\n");
        break;
      case ActionExecutor::RUNNING:
        fprintf(stderr, "\tRUNNING\n");
        break;
      case ActionExecutor::SUCCESS:
        fprintf(stderr, "\tSUCCESS\n");
        break;
      case ActionExecutor::FAILURE:
        fprintf(stderr, "\tFAILURE\n");
        break;
    }
    if (action_info.second.durative_action_info == nullptr) {
      fprintf(stderr, "\tWith no duration info\n");
    }

    if (action_info.second.at_start_effects_applied) {
      fprintf(stderr, "\tAt start effects applied\n");
    } else {
      fprintf(stderr, "\tAt start effects NOT applied\n");
    }

    if (action_info.second.at_end_effects_applied) {
      fprintf(stderr, "\tAt end effects applied\n");
    } else {
      fprintf(stderr, "\tAt end effects NOT applied\n");
    }
  }
}

}  // namespace plansys2
