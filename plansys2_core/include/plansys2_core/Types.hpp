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

#ifndef PLANSYS2_CORE__TYPES_HPP_
#define PLANSYS2_CORE__TYPES_HPP_

#include <string>
#include <vector>

#include <plansys2_msgs/Node.h>
#include <plansys2_msgs/Param.h>
#include <plansys2_msgs/Tree.h>

#include <plansys2_pddl_parser/Utils.h>

namespace plansys2
{

class Instance : public plansys2_msgs::Param
{
public:
  Instance()
  : plansys2_msgs::Param() {}
  explicit Instance(const std::string & name, const std::string & type = {})
  : plansys2_msgs::Param(parser::pddl::fromStringParam(name, type)) {}
  Instance(const plansys2_msgs::Param & instance)  // NOLINT(runtime/explicit)
  : plansys2_msgs::Param(instance) {}
};

class Predicate : public plansys2_msgs::Node
{
public:
  Predicate()
  : plansys2_msgs::Node() {}
  explicit Predicate(const std::string & pred)
  : plansys2_msgs::Node(parser::pddl::fromStringPredicate(pred)) {}
  Predicate(const plansys2_msgs::Node & pred)  // NOLINT(runtime/explicit)
  : plansys2_msgs::Node(pred) {}
};

class Function : public plansys2_msgs::Node
{
public:
  Function()
  : plansys2_msgs::Node() {}
  explicit Function(const std::string & func)
  : plansys2_msgs::Node(parser::pddl::fromStringFunction(func)) {}
  Function(const plansys2_msgs::Node & func)  // NOLINT(runtime/explicit)
  : plansys2_msgs::Node(func) {}
};

class Goal : public plansys2_msgs::Tree
{
public:
  Goal()
  : plansys2_msgs::Tree() {}
  explicit Goal(const std::string & goal)
  : plansys2_msgs::Tree(parser::pddl::fromString(goal)) {}
  Goal(const plansys2_msgs::Tree & goal)  // NOLINT(runtime/explicit)
  : plansys2_msgs::Tree(goal) {}
};

template<class toT, class fromT>
std::vector<toT>
convertVector(const std::vector<fromT> & in_vector)
{
  std::vector<toT> ret;
  for (const auto & item : in_vector) {
    ret.push_back(item);
  }
  return ret;
}

}  // namespace plansys2

#endif  // PLANSYS2_CORE__TYPES_HPP_
