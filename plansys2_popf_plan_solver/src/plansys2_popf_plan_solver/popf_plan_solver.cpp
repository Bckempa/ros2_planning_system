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

#include <sys/stat.h>
#include <sys/types.h>

#include <filesystem>
#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ros/package.h>

#include <plansys2_msgs/PlanItem.h>
#include <plansys2_popf_plan_solver/popf_plan_solver.hpp>

namespace plansys2
{

POPFPlanSolver::POPFPlanSolver()
{
}

void POPFPlanSolver::configure(std::shared_ptr<ros::lifecycle::ManagedNode> & lc_node,
			       const std::string & plugin_name)
{
  parameter_name_ = plugin_name + "/arguments";
  lc_node_ = lc_node;
  // Backport: No need to declare parameters in ROS1
  //lc_node_->declare_parameter<std::string>(parameter_name_, "");
}

std::optional<plansys2_msgs::Plan>
POPFPlanSolver::getPlan(const std::string & domain,
			const std::string & problem,
			const std::string & node_namespace)
{
  if (system(nullptr) == 0) {
    return {};
  }

  if (node_namespace != "") {
    std::filesystem::path tp = std::filesystem::temp_directory_path();
    for (auto p : std::filesystem::path(node_namespace) ) {
      if (p != std::filesystem::current_path().root_directory()) {
        tp /= p;
      }
    }
    std::filesystem::create_directories(tp);
  }

  plansys2_msgs::Plan ret;
  std::ofstream domain_out("/tmp/" + node_namespace + "/domain.pddl");
  domain_out << domain;
  domain_out.close();

  std::ofstream problem_out("/tmp/" + node_namespace + "/problem.pddl");
  problem_out << problem;
  problem_out.close();


  std::string extra_params;
  lc_node_->getBaseNode().getParam(parameter_name_, extra_params);

  std::string popf_bin = "rosrun popf popf";
  // lc_node_->getBaseNode().param<std::string>("popf_bin", popf_bin, "rosrun popf popf");

  int status = system(
    (popf_bin + " " +
      extra_params +
    " /tmp/" + node_namespace + "/domain.pddl /tmp/" + node_namespace +
    "/problem.pddl > /tmp/" + node_namespace + "/plan").c_str());

  if (status == -1) {
    return {};
  }

  std::string line;
  std::ifstream plan_file("/tmp/" + node_namespace + "/plan");
  bool solution = false;

  if (plan_file.is_open()) {
    while (getline(plan_file, line)) {
      if (!solution) {
        if (line.find("Solution Found") != std::string::npos) {
          solution = true;
        }
      } else if (line.front() != ';') {
        plansys2_msgs::PlanItem item;
        size_t colon_pos = line.find(":");
        size_t colon_par = line.find(")");
        size_t colon_bra = line.find("[");

        std::string time = line.substr(0, colon_pos);
        std::string action = line.substr(colon_pos + 2, colon_par - colon_pos - 1);
        std::string duration = line.substr(colon_bra + 1);
        duration.pop_back();

        item.time = std::stof(time);
        item.action = action;
        item.duration = std::stof(duration);

        ret.items.push_back(item);
      }
    }
    plan_file.close();
  }

  if (ret.items.empty()) {
    return {};
  }

  return ret;
}

bool
POPFPlanSolver::is_valid_domain(
  const std::string & domain,
  const std::string & node_namespace)
{
  if (system(nullptr) == 0) {
    return false;
  }

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
  if (node_namespace != "") {
    for (auto p : std::filesystem::path(node_namespace) ) {
      if (p != std::filesystem::current_path().root_directory()) {
        temp_dir /= p;
      }
    }
    std::filesystem::create_directories(temp_dir);
  }

  std::ofstream domain_out(temp_dir.string() + "/check_domain.pddl");
  domain_out << domain;
  domain_out.close();

  std::ofstream problem_out(temp_dir.string() + "/check_problem.pddl");
  problem_out << "(define (problem void) (:domain plansys2))\n";
  problem_out.close();

  std::string popf_bin = "rosrun popf popf";
  // lc_node_->getBaseNode().param<std::string>("popf_bin", popf_bin, "rosrun popf popf");

  std::string command_line = std::string(popf_bin + " " + temp_dir.string() + "/check_domain.pddl " + temp_dir.string() +
    "/check_problem.pddl > " + temp_dir.string() + "/check.out");
    
  int status = system( command_line.c_str() );

  if (status == -1) {
    return false;
  }

  std::string line;
  std::ifstream plan_file(temp_dir.string() + "/check.out");
  bool solution = false;
  bool domain_error = false;
  bool problem_error = false;
  if (plan_file && plan_file.is_open()) {
    while (getline(plan_file, line)) {
      if (!solution) {
        if (line.find("Solution Found") != std::string::npos) {
          solution = true;
        }
      }
      
      if (line.find("Syntax error in domain") != std::string::npos)
        domain_error = true;
        
      if (line.find("Syntax error in problem definition") != std::string::npos)
        problem_error = true;
        
    }
    plan_file.close();
  }

  if(solution || (!domain_error && problem_error) )
    return true;

  return false;
}

}  // namespace plansys2

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(plansys2::POPFPlanSolver, plansys2::PlanSolverBase)
