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

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <plansys2_executor/bt_builder_plugins/stn_bt_builder.hpp>
#include <plansys2_problem_expert/Utils.hpp>

namespace plansys2
{

STNBTBuilder::STNBTBuilder()
{
  domain_client_ = std::make_shared<plansys2::DomainExpertClient>();
  problem_client_ = std::make_shared<plansys2::ProblemExpertClient>();
}

void
STNBTBuilder::initialize(
  const std::string & bt_action_1,
  const std::string & bt_action_2,
  int precision)
{
  if (bt_action_1 != "") {
    bt_start_action_ = bt_action_1;
  } else {
    bt_start_action_ =
      R""""(<Sequence name="ACTION_ID">
WAIT_PREV_ACTIONS
  <WaitAtStartReq action="ACTION_ID"/>
  <ApplyAtStartEffect action="ACTION_ID"/>
</Sequence>
)"""";
  }

  if (bt_action_2 != "") {
    bt_end_action_ = bt_action_2;
  } else {
    bt_end_action_ =
      R""""(<Sequence name="ACTION_ID">
  <ReactiveSequence name="ACTION_ID">
  <CheckOverAllReq action="ACTION_ID"/>
    <ExecuteAction action="ACTION_ID"/>
  </ReactiveSequence>
CHECK_PREV_ACTIONS
  <CheckAtEndReq action="ACTION_ID"/>
  <ApplyAtEndEffect action="ACTION_ID"/>
</Sequence>
)"""";
  }

  action_time_precision_ = precision;
}

std::string
STNBTBuilder::get_tree(const plansys2_msgs::Plan & plan)
{
  stn_ = build_stn(plan);
  auto bt = build_bt(stn_);
  return bt;
}

std::string
STNBTBuilder::get_dotgraph(
  std::shared_ptr<std::map<std::string, ActionExecutionInfo>> action_map,
  bool enable_legend,
  bool enable_print_graph)
{
  if (enable_print_graph) {
    print_graph(stn_);
  }

  // create xdot graph
  std::stringstream ss;
  ss << "digraph plan {\n";

  // dotgraph formatting options
  ss << t(1) << "node[shape=box];\n";
  ss << t(1) << "rankdir=TB;\n";

  int max_level = 0;
  int max_node = 0;

  // Perform breadth first search
  std::queue<std::pair<GraphNode::Ptr, int>> queue;
  stn_->nodes.front()->visited = true;
  queue.push(std::make_pair(stn_->nodes.front(), 0));

  int prev_level = -1;
  while (!queue.empty()) {
    auto pair = queue.front();
    auto node = pair.first;
    auto level = pair.second;
    if (level != prev_level) {
      ss << t(1) << "subgraph cluster_" << level << " {\n";
      auto start_time = node->action.time;
      if (node->action.type == ActionType::END) {
        start_time += node->action.duration;
      }
      ss << t(2) << "label = \"Time: " << start_time << "\";\n";
      ss << t(2) << "style = rounded;\n";
      ss << t(2) << "color = yellow3;\n";
      ss << t(2) << "bgcolor = lemonchiffon;\n";
      ss << t(2) << "labeljust = l;\n";
      max_level = std::max(max_level, level);
    }
    max_node = std::max(max_node, node->node_num);
    ss << get_node_dotgraph(node, action_map);
    ss << t(1) << "}\n";
    prev_level = level;
    queue.pop();
    for (auto arc : node->output_arcs) {
      auto child = std::get<0>(arc);
      if (!child->visited) {
        child->visited = true;
        queue.push(std::make_pair(child, level + 1));
      }
    }
  }

  // Reset visited flag.
  for (auto node : stn_->nodes) {
    node->visited = false;
  }

  // define the edges
  std::set<std::string> edges;
  for (const auto & graph_root : stn_->nodes) {
    get_flow_dotgraph(graph_root, edges);
  }

  for (const auto & edge : edges) {
    ss << t(1) << edge;
  }

  if (enable_legend) {
    max_level++;
    max_node++;
    ss << add_dot_graph_legend(max_level, max_node);
  }

  ss << "}";

  return ss.str();
}

Graph::Ptr
STNBTBuilder::build_stn(const plansys2_msgs::Plan & plan) const
{
  auto stn = init_graph(plan);
  auto happenings = get_happenings(plan);
  auto simple_plan = get_simple_plan(plan);
  auto states = get_states(happenings, simple_plan);

  for (const auto & item : simple_plan) {
    // Skip the first action corresponding to the initial state
    if (item.first < 0) {
      continue;
    }

    // Get the graph nodes corresponding to the current action
    auto current = get_nodes(item.second, stn);

    // Get the parent actions of the current action
    auto parents = get_parents(item, simple_plan, happenings, states);

    for (const auto & parent : parents) {
      auto previous = get_nodes(parent.second, stn);
      for (auto & n : current) {
        for (auto & h : previous) {
          prune_paths(n, h);
          if (!check_paths(n, h)) {
            h->output_arcs.insert(std::make_tuple(n, 0, std::numeric_limits<float>::infinity()));
            n->input_arcs.insert(std::make_tuple(h, 0, std::numeric_limits<float>::infinity()));
          }
        }
      }
    }
  }

  return stn;
}

std::string
STNBTBuilder::build_bt(const Graph::Ptr stn) const
{
  std::set<GraphNode::Ptr> used;
  const auto & root = stn->nodes.front();

  auto bt = std::string("<root BTCPP_format=\"4\" main_tree_to_execute=\"MainTree\">\n") + t(1) +
    "<BehaviorTree ID=\"MainTree\">\n";
  bt = bt + get_flow(root, root, used, 1);
  bt = bt + t(1) + "</BehaviorTree>\n</root>\n";

  return bt;
}

Graph::Ptr
STNBTBuilder::init_graph(const plansys2_msgs::Plan & plan) const
{
  auto graph = Graph::make_shared();
  auto action_sequence = get_plan_actions(plan);

  // Add a node to represent the initial state
  auto predicates = problem_client_->getPredicates();
  auto functions = problem_client_->getFunctions();

  int node_cnt = 0;
  auto init_node = GraphNode::make_shared(node_cnt++);
  init_node->action.action = std::make_shared<plansys2_msgs::DurativeAction>();
  init_node->action.action->at_end_effects = from_state(predicates, functions);
  init_node->action.type = ActionType::INIT;
  graph->nodes.push_back(init_node);

  // Add nodes for the start and end snap actions
  for (const auto & action : action_sequence) {
    auto start_node = GraphNode::make_shared(node_cnt++);
    start_node->action = action;
    start_node->action.type = ActionType::START;

    auto end_node = GraphNode::make_shared(node_cnt++);
    end_node->action = action;
    end_node->action.type = ActionType::END;

    start_node->output_arcs.insert(std::make_tuple(end_node, action.duration, action.duration));
    end_node->input_arcs.insert(std::make_tuple(start_node, action.duration, action.duration));

    graph->nodes.push_back(start_node);
    graph->nodes.push_back(end_node);
  }

  // Add a node to represent the goal
  auto goal = problem_client_->getGoal();
  plansys2_msgs::Tree * goal_tree = &goal;

  auto goal_node = GraphNode::make_shared(node_cnt++);
  goal_node->action.action = std::make_shared<plansys2_msgs::DurativeAction>();
  goal_node->action.action->at_start_requirements = *goal_tree;
  goal_node->action.type = ActionType::GOAL;
  graph->nodes.push_back(goal_node);

  return graph;
}

std::vector<ActionStamped>
STNBTBuilder::get_plan_actions(const plansys2_msgs::Plan & plan) const
{
  std::vector<ActionStamped> ret;

  for (auto & item : plan.items) {
    ActionStamped action_stamped;
    action_stamped.time = item.time;
    action_stamped.expression = item.action;
    action_stamped.duration = item.duration;
    action_stamped.type = ActionType::DURATIVE;
    action_stamped.action =
      domain_client_->getDurativeAction(
      get_action_name(item.action), get_action_params(item.action));

    ret.push_back(action_stamped);
  }

  return ret;
}

std::set<int>
STNBTBuilder::get_happenings(const plansys2_msgs::Plan & plan) const
{
  std::set<int> happenings;
  happenings.insert(-1);
  auto action_sequence = get_plan_actions(plan);
  for (const auto & action : action_sequence) {
    auto time = to_int_time(action.time, action_time_precision_ + 1);
    auto duration = to_int_time(action.duration, action_time_precision_ + 1);
    happenings.insert(time);
    happenings.insert(time + duration);
  }
  return happenings;
}

std::set<int>::iterator
STNBTBuilder::get_happening(int time, const std::set<int> & happenings) const
{
  // This function returns an iterator pointing to either
  //   1. the first element that is equal to the key or
  //   2. the last element that is less than the key,
  // whichever of the two is greater.

  // lower_bound returns an iterator pointing to the first element that is
  // not less than (i.e. greater or equal to) key.
  auto it = happenings.lower_bound(time);
  if (it != happenings.end()) {
    if (*it != time) {
      if (it != happenings.begin()) {
        it--;
      } else {
        it = happenings.end();
      }
    }
  }

  return it;
}

std::set<int>::iterator
STNBTBuilder::get_previous(int time, const std::set<int> & happenings) const
{
  auto it = get_happening(time, happenings);

  if (it != happenings.end()) {
    if (it != happenings.begin()) {
      return std::prev(it);
    }
  }

  return happenings.end();
}

std::multimap<int, ActionStamped>
STNBTBuilder::get_simple_plan(const plansys2_msgs::Plan & plan) const
{
  std::multimap<int, ActionStamped> simple_plan;
  auto action_sequence = get_plan_actions(plan);

  // Add an action to represent the initial state
  auto predicates = problem_client_->getPredicates();
  auto functions = problem_client_->getFunctions();

  ActionStamped init_action;
  init_action.action = std::make_shared<plansys2_msgs::DurativeAction>();
  init_action.action->at_end_effects = from_state(predicates, functions);
  init_action.type = ActionType::INIT;
  simple_plan.insert(std::make_pair(-1, init_action));

  // Add the snap actions
  for (auto action : action_sequence) {
    auto time = to_int_time(action.time, action_time_precision_ + 1);
    auto duration = to_int_time(action.duration, action_time_precision_ + 1);
    action.type = ActionType::START;
    simple_plan.insert(std::make_pair(time, action));
    action.type = ActionType::END;
    simple_plan.insert(std::make_pair(time + duration, action));
  }

  // Create the overall actions
  std::vector<std::pair<int, ActionStamped>> overall_actions;
  for (const auto & action : action_sequence) {
    auto time = to_int_time(action.time, action_time_precision_ + 1);
    auto duration = to_int_time(action.duration, action_time_precision_ + 1);

    // Find the start action
    auto it = simple_plan.equal_range(time);
    auto start_action = std::find_if(
      it.first, it.second,
      [&](std::pair<int, ActionStamped> a) {
        return (a.second.expression == action.expression) && (a.second.type == ActionType::START);
      });

    // Find the end action
    it = simple_plan.equal_range(time + duration);
    auto end_action = std::find_if(
      it.first, it.second,
      [&](std::pair<int, ActionStamped> a) {
        return (a.second.expression == action.expression) && (a.second.type == ActionType::END);
      });

    // Compute the overall actions
    int prev = time;
    for (auto iter = start_action; iter != simple_plan.end(); ++iter) {
      if (iter->first != prev) {
        int time = prev + (iter->first - prev) / 2;
        auto overall_action = std::make_pair(time, start_action->second);
        overall_action.second.type = ActionType::OVERALL;
        overall_actions.push_back(overall_action);
        prev = iter->first;
      }
      if (iter == end_action) {
        break;
      }
    }
  }

  // Add the overall actions
  for (const auto & overall_action : overall_actions) {
    simple_plan.insert(overall_action);
  }

  return simple_plan;
}

std::map<int, StateVec>
STNBTBuilder::get_states(
  const std::set<int> & happenings,
  const std::multimap<int, ActionStamped> & plan) const
{
  std::map<int, StateVec> states;

  StateVec state_vec;
  state_vec.predicates = problem_client_->getPredicates();
  state_vec.functions = problem_client_->getFunctions();
  states.insert(std::make_pair(-1, state_vec));

  for (const auto & time : happenings) {
    auto it = plan.equal_range(time);
    for (auto iter = it.first; iter != it.second; ++iter) {
      if (iter->second.type == ActionType::START) {
        apply(iter->second.action->at_start_effects, state_vec.predicates, state_vec.functions);
      } else if (iter->second.type == ActionType::END) {
        apply(iter->second.action->at_end_effects, state_vec.predicates, state_vec.functions);
      }
    }
    states.insert(std::make_pair(time, state_vec));
  }

  return states;
}

plansys2_msgs::Tree
STNBTBuilder::from_state(
  const std::vector<plansys2::Predicate> & preds,
  const std::vector<plansys2::Function> & funcs) const
{
  plansys2_msgs::Tree tree;
  plansys2_msgs::Node node;
  node.node_type = plansys2_msgs::Node::AND;
  node.node_id = 0;
  node.negate = false;
  tree.nodes.push_back(node);

  for (const auto & pred : preds) {
    const plansys2_msgs::Node * child = &pred;
    tree.nodes.push_back(*child);
    tree.nodes.back().node_id = tree.nodes.size() - 1;
    tree.nodes[0].children.push_back(tree.nodes.size() - 1);
  }

  for (const auto & func : funcs) {
    const plansys2_msgs::Node * child = &func;
    tree.nodes.push_back(*child);
    tree.nodes.back().node_id = tree.nodes.size() - 1;
    tree.nodes[0].children.push_back(tree.nodes.size() - 1);
  }

  return tree;
}

std::vector<GraphNode::Ptr>
STNBTBuilder::get_nodes(
  const ActionStamped & action,
  const Graph::Ptr & graph) const
{
  std::vector<GraphNode::Ptr> ret;

  if (action.type == ActionType::INIT) {
    auto it = std::find_if(
      graph->nodes.begin(), graph->nodes.end(), [&](GraphNode::Ptr node) {
        return node->action.type == ActionType::INIT;
      });
    if (it != graph->nodes.end()) {
      ret.push_back(*it);
    } else {
      std::cerr << "get_nodes: Could not find initial state node" << std::endl;
    }
    return ret;
  }

  std::vector<GraphNode::Ptr> matches;
  std::copy_if(
    graph->nodes.begin(), graph->nodes.end(), std::back_inserter(matches),
    std::bind(&STNBTBuilder::is_match, this, std::placeholders::_1, action));

  if (action.type == ActionType::START || action.type == ActionType::OVERALL) {
    auto it = std::find_if(
      matches.begin(), matches.end(), [&](GraphNode::Ptr node) {
        return node->action.type == ActionType::START;
      });
    if (it != matches.end()) {
      ret.push_back(*it);
    } else {
      std::cerr << "get_nodes: Could not find start node" << std::endl;
    }
  }

  if (action.type == ActionType::END || action.type == ActionType::OVERALL) {
    auto it = std::find_if(
      matches.begin(), matches.end(), [&](GraphNode::Ptr node) {
        return node->action.type == ActionType::END;
      });
    if (it != matches.end()) {
      ret.push_back(*it);
    } else {
      std::cerr << "get_nodes: Could not find end node" << std::endl;
    }
  }

  if (ret.empty()) {
    std::cerr << "get_nodes: Could not find graph node" << std::endl;
  }

  return ret;
}

bool
STNBTBuilder::is_match(
  const GraphNode::Ptr node,
  const ActionStamped & action) const
{
  auto t_1 = to_int_time(node->action.time, action_time_precision_ + 1);
  auto t_2 = to_int_time(action.time, action_time_precision_ + 1);
  return (t_1 == t_2) && (node->action.expression == action.expression);
}

std::vector<std::pair<int, ActionStamped>>
STNBTBuilder::get_parents(
  const std::pair<int, ActionStamped> & action,
  const std::multimap<int, ActionStamped> & plan,
  const std::set<int> & happenings,
  const std::map<int, StateVec> & states) const
{
  auto parents = get_satisfy(action, plan, happenings, states);
  auto threats = get_threat(action, plan, happenings, states);
  parents.insert(std::end(parents), std::begin(threats), std::end(threats));

  return parents;
}

std::vector<std::pair<int, ActionStamped>>
STNBTBuilder::get_satisfy(
  const std::pair<int, ActionStamped> & action,
  const std::multimap<int, ActionStamped> & plan,
  const std::set<int> & happenings,
  const std::map<int, StateVec> & states) const
{
  std::vector<std::pair<int, ActionStamped>> ret;

  auto H_it = get_happening(action.first, happenings);
  if (H_it == happenings.end()) {
    std::cerr << "get_satisfy: Happening time not found" << std::endl;
    return ret;
  }
  auto t_2 = *H_it;

  auto R_a = parser::pddl::getSubtrees(get_conditions(action.second));

  while (t_2 >= 0) {
    H_it = get_previous(t_2, happenings);
    if (H_it == happenings.end()) {
      std::cerr << "get_satisfy: Previous happening time not found" << std::endl;
      break;
    }
    auto t_1 = *H_it;

    auto X_it = states.find(t_1);
    if (X_it == states.end()) {
      std::cerr << "get_satisfy: Previous state not found" << std::endl;
      break;
    }
    auto X_1 = X_it->second;

    for (const auto & r : R_a) {
      if (!check(r, X_1.predicates, X_1.functions)) {
        auto it = plan.equal_range(t_2);
        for (auto iter = it.first; iter != it.second; ++iter) {
          if (iter->first == action.first) {
            if (iter->second.expression == action.second.expression) {
              continue;
            }
          }

          auto E_k = get_effects(iter->second);

          auto X_hat = X_1;
          apply(E_k, X_hat.predicates, X_hat.functions);

          // Check if action k satisfies action i
          if (check(r, X_hat.predicates, X_hat.functions)) {
            ret.push_back(*iter);
          }
        }
      }
    }
    t_2 = t_1;
  }

  auto predicates = problem_client_->getPredicates();
  auto functions = problem_client_->getFunctions();

  for (const auto & r : R_a) {
    if (check(r, predicates, functions)) {
      ret.push_back(*plan.begin());
    }
  }

  return ret;
}

std::vector<std::pair<int, ActionStamped>>
STNBTBuilder::get_threat(
  const std::pair<int, ActionStamped> & action,
  const std::multimap<int, ActionStamped> & plan,
  const std::set<int> & happenings,
  const std::map<int, StateVec> & states) const
{
  std::vector<std::pair<int, ActionStamped>> ret;

  auto H_it = get_happening(action.first, happenings);
  if (H_it == happenings.end()) {
    std::cerr << "get_threat: Happening time not found" << std::endl;
    return ret;
  }
  auto t_in = *H_it;
  auto t_2 = *H_it;

  auto R_a = get_conditions(action.second);
  auto E_a = get_effects(action.second);

  while (t_2 >= 0) {
    H_it = get_previous(t_2, happenings);
    if (H_it == happenings.end()) {
      std::cerr << "get_threat: Previous happening time not found" << std::endl;
      break;
    }
    auto t_1 = *H_it;

    auto X_it = states.find(t_1);
    if (X_it == states.end()) {
      std::cerr << "get_threat: Previous state not found" << std::endl;
      break;
    }
    auto X_1 = X_it->second;

    auto X_1_a = X_1;
    if (can_apply(action, plan, t_2, X_1_a)) {
      auto it = plan.end();
      H_it = happenings.find(t_2);
      if (std::next(H_it) != happenings.end()) {
        it = plan.lower_bound(*std::next(H_it));
      }

      for (auto iter = plan.lower_bound(t_2); iter != it; ++iter) {
        if (iter->first == action.first) {
          if (iter->second.expression == action.second.expression) {
            continue;
          }
        }

        auto X_1_k = X_1;
        if (!can_apply(*iter, plan, t_2, X_1_k)) {
          std::cerr << "get_threat: Suitable intermediate state not found. ";
          std::cerr << "However, one should exist." << std::endl;
          continue;
        }

        auto R_k = get_conditions(iter->second);
        auto E_k = get_effects(iter->second);

        auto X_hat = X_1_k;
        apply(E_a, X_hat.predicates, X_hat.functions);

        // Check if the input action threatens action k
        if (action.second.type != ActionType::OVERALL &&
          !check(R_k, X_hat.predicates, X_hat.functions))
        {
          if (t_2 != t_in) {
            ret.push_back(*iter);
          } else {
            std::cerr << "get_threat: An action should not be threatened ";
            std::cerr << "by another action at the same happening time. ";
            std::cerr << "Check the plan validity." << std::endl;
          }
          continue;
        }

        auto X_bar = X_1_a;
        apply(E_k, X_bar.predicates, X_bar.functions);

        // Check if action k threatens the input action
        if (iter->second.type != ActionType::OVERALL &&
          !check(R_a, X_bar.predicates, X_bar.functions))
        {
          if (t_2 != t_in) {
            ret.push_back(*iter);
          } else {
            std::cerr << "get_threat: An action should not be threatened ";
            std::cerr << "by another action at the same happening time. ";
            std::cerr << "Check the plan validity." << std::endl;
          }
          continue;
        }

        // Check if the input action and action k modify the same effect
        if (action.second.type != ActionType::OVERALL &&
          iter->second.type != ActionType::OVERALL)
        {
          auto DX_hat = get_diff(X_1_k, X_hat);
          auto DX_bar = get_diff(X_1_a, X_bar);
          auto intersection = get_intersection(DX_hat, DX_bar);
          if (intersection.predicates.size() > 0 || intersection.functions.size() > 0) {
            if (t_2 != t_in) {
              ret.push_back(*iter);
            } else {
              std::cerr << "get_threat: An action should not be threatened ";
              std::cerr << "by another action at the same happening time. ";
              std::cerr << "Check the plan validity." << std::endl;
            }
          }
        }
      }
    }
    t_2 = t_1;
  }

  return ret;
}

bool
STNBTBuilder::can_apply(
  const std::pair<int, ActionStamped> & action,
  const std::multimap<int, ActionStamped> & plan,
  const int & time,
  StateVec & state) const
{
  auto X = state;
  auto R = get_conditions(action.second);

  if (check(R, X.predicates, X.functions)) {
    return true;
  }

  int n = plan.count(time);
  auto it = plan.lower_bound(time);
  for (int m = 1; m <= n; ++m) {
    std::vector<bool> v(n);
    std::fill(v.begin(), v.begin() + m, true);

    state = X;
    do {
      for (int i = 0; i < n; ++i) {
        if (v[i]) {
          auto iter = std::next(it, i);
          if (iter->first == action.first) {
            if (iter->second.expression == action.second.expression) {
              continue;
            }
          }
          auto E = get_effects(iter->second);
          apply(E, state.predicates, state.functions);
          if (check(R, state.predicates, state.functions)) {
            return true;
          }
        }
      }
    } while (std::prev_permutation(v.begin(), v.end()));
  }

  return false;
}

StateVec
STNBTBuilder::get_diff(
  const StateVec & X_1,
  const StateVec & X_2) const
{
  StateVec ret;

  // Look for predicates in X_1 that are not in X_2
  for (const auto & p_1 : X_1.predicates) {
    auto it = std::find_if(
      X_2.predicates.begin(), X_2.predicates.end(),
      [&](plansys2::Predicate p_2) {
        return parser::pddl::checkNodeEquality(p_1, p_2);
      });
    if (it == X_2.predicates.end()) {
      ret.predicates.push_back(p_1);
    }
  }

  // Look for predicates in X_2 that are not in X_1
  for (const auto & p_2 : X_2.predicates) {
    auto it = std::find_if(
      X_1.predicates.begin(), X_1.predicates.end(),
      [&](plansys2::Predicate p_1) {
        return parser::pddl::checkNodeEquality(p_1, p_2);
      });
    if (it == X_1.predicates.end()) {
      ret.predicates.push_back(p_2);
    }
  }

  // Look for function changes
  for (const auto & f_1 : X_1.functions) {
    auto it = std::find_if(
      X_2.functions.begin(), X_2.functions.end(),
      [&](plansys2::Function f_2) {
        return parser::pddl::checkNodeEquality(f_1, f_2);
      });
    if (it != X_2.functions.end()) {
      if (std::abs(f_1.value - it->value) >
        1e-5 * std::max(std::abs(f_1.value), std::abs(it->value)))
      {
        ret.functions.push_back(f_1);
      }
    }
  }

  return ret;
}

StateVec
STNBTBuilder::get_intersection(
  const StateVec & X_1,
  const StateVec & X_2) const
{
  StateVec ret;

  // Look for predicates in X_1 that are also in X_2
  for (const auto & p_1 : X_1.predicates) {
    auto it = std::find_if(
      X_2.predicates.begin(), X_2.predicates.end(),
      [&](plansys2::Predicate p_2) {
        return parser::pddl::checkNodeEquality(p_1, p_2);
      });
    if (it != X_2.predicates.end()) {
      ret.predicates.push_back(p_1);
    }
  }

  // Look for functions in X_1 that are also in X_2
  for (const auto & f_1 : X_1.functions) {
    auto it = std::find_if(
      X_2.functions.begin(), X_2.functions.end(),
      [&](plansys2::Function f_2) {
        return parser::pddl::checkNodeEquality(f_1, f_2);
      });
    if (it != X_2.functions.end()) {
      ret.functions.push_back(f_1);
    }
  }

  return ret;
}

plansys2_msgs::Tree
STNBTBuilder::get_conditions(const ActionStamped & action) const
{
  if (action.type == ActionType::START) {
    return action.action->at_start_requirements;
  } else if (action.type == ActionType::OVERALL) {
    return action.action->over_all_requirements;
  } else if (action.type == ActionType::END) {
    return action.action->at_end_requirements;
  }

  return plansys2_msgs::Tree();
}

plansys2_msgs::Tree
STNBTBuilder::get_effects(const ActionStamped & action) const
{
  if (action.type == ActionType::START) {
    return action.action->at_start_effects;
  } else if (action.type == ActionType::INIT || action.type == ActionType::END) {
    return action.action->at_end_effects;
  }

  return plansys2_msgs::Tree();
}

void
STNBTBuilder::prune_paths(GraphNode::Ptr current, GraphNode::Ptr previous) const
{
  // Traverse the graph from the previous node to the root
  for (auto & in : previous->input_arcs) {
    prune_paths(current, std::get<0>(in));
  }

  // Don't remove the link between the start and end node
  if (previous->action.time == current->action.time) {
    if (previous->action.expression == current->action.expression) {
      if (previous->action.type != ActionType::START) {
        std::cerr << "prune_paths: Expected previous action to be of type START" << std::endl;
      }
      if (current->action.type != ActionType::END) {
        std::cerr << "prune_paths: Expected current action to be of type END" << std::endl;
      }
      return;
    }
  }

  auto it = previous->output_arcs.begin();
  while (it != previous->output_arcs.end()) {
    // Check for an output link to current
    if (std::get<0>(*it) == current) {
      // Find the corresponding input link
      auto in = std::find_if(
        current->input_arcs.begin(), current->input_arcs.end(),
        [&](std::tuple<GraphNode::Ptr, double, double> arc) {
          return std::get<0>(arc) == previous;
        });
      // Remove the output and input links
      if (in != current->input_arcs.end()) {
        current->input_arcs.erase(in);
        it = previous->output_arcs.erase(it);
      } else {
        std::cerr << "prune_backards: Input arc could not be found" << std::endl;
      }
    } else {
      ++it;
    }
  }
}

bool
STNBTBuilder::check_paths(GraphNode::Ptr current, GraphNode::Ptr previous) const
{
  // Traverse the graph from the current node to the root
  for (auto & in : current->input_arcs) {
    if (check_paths(std::get<0>(in), previous)) {
      return true;
    }
  }

  // Check if the current node is equal to the previous node
  if (current == previous) {
    return true;
  }

  return false;
}

std::string
STNBTBuilder::get_flow(
  const GraphNode::Ptr node,
  const GraphNode::Ptr parent,
  std::set<GraphNode::Ptr> & used,
  const int & level) const
{
  int l = level;
  const auto action_id = to_action_id(node->action, action_time_precision_);

  if (used.find(node) != used.end()) {
    return t(l) + "<WaitAction action=\"" + action_id + "\"/>\n";
  }

  used.insert(node);

  if (node->output_arcs.size() == 0) {
    if (node->action.type == ActionType::END) {
      return end_execution_block(node, parent, l);
    }
    std::cerr << "get_flow: Unexpected action type" << std::endl;
    return {};
  }

  std::string flow;

  if (node->action.type != ActionType::INIT) {
    flow = flow + t(l) + "<Sequence name=\"" + action_id + "\">\n";
  }

  if (node->action.type == ActionType::START) {
    flow = flow + start_execution_block(node, parent, l + 1);
  } else if (node->action.type == ActionType::END) {
    flow = flow + end_execution_block(node, parent, l + 1);
  }

  int n = 0;
  if (node->output_arcs.size() > 1) {
    flow = flow + t(l + 1) +
      "<Parallel success_count=\"" + std::to_string(node->output_arcs.size()) +
      "\" failure_count=\"1\">\n";
    n = 1;
  }

  // Visit the end action first
  if (node->action.type == ActionType::START) {
    auto end_action = std::find_if(
      node->output_arcs.begin(), node->output_arcs.end(),
      std::bind(&STNBTBuilder::is_end, this, std::placeholders::_1, node->action));
    if (end_action != node->output_arcs.end()) {
      const auto & next = std::get<0>(*end_action);
      flow = flow + get_flow(next, node, used, l + n + 1);
    }
  }

  // Visit the rest of the output arcs
  for (const auto & child : node->output_arcs) {
    if (!is_end(child, node->action)) {
      const auto & next = std::get<0>(child);
      flow = flow + get_flow(next, node, used, l + n + 1);
    }
  }

  if (node->output_arcs.size() > 1) {
    flow = flow + t(l + 1) + "</Parallel>\n";
  }

  if (node->action.type != ActionType::INIT) {
    flow = flow + t(l) + "</Sequence>\n";
  }

  return flow;
}

std::string
STNBTBuilder::start_execution_block(
  const GraphNode::Ptr node,
  const GraphNode::Ptr parent,
  const int & l) const
{
  std::string ret;
  std::string ret_aux = bt_start_action_;
  const std::string action_id = to_action_id(node->action, action_time_precision_);

  std::string wait_actions;
  for (const auto & prev : node->input_arcs) {
    const auto & prev_node = std::get<0>(prev);
    if (prev_node != parent) {
      wait_actions = wait_actions + t(1) + "<WaitAction action=\"" +
        to_action_id(prev_node->action, action_time_precision_) + "\"/>";
    }

    if (prev != *node->input_arcs.rbegin()) {
      wait_actions = wait_actions + "\n";
    }
  }

  replace(ret_aux, "ACTION_ID", action_id);
  replace(ret_aux, "WAIT_PREV_ACTIONS", wait_actions);

  std::istringstream f(ret_aux);
  std::string line;
  while (std::getline(f, line)) {
    if (line != "") {
      ret = ret + t(l) + line + "\n";
    }
  }
  return ret;
}

std::string
STNBTBuilder::end_execution_block(
  const GraphNode::Ptr node,
  const GraphNode::Ptr parent,
  const int & l) const
{
  std::string ret;
  std::string ret_aux = bt_end_action_;
  const std::string action_id = to_action_id(node->action, action_time_precision_);

  std::string check_actions;
  for (const auto & prev : node->input_arcs) {
    const auto & prev_node = std::get<0>(prev);
    if (prev_node != parent) {
      check_actions = check_actions + t(1) + "<CheckAction action=\"" +
        to_action_id(prev_node->action, action_time_precision_) + "\"/>";
    }

    if (prev != *node->input_arcs.rbegin()) {
      check_actions = check_actions + "\n";
    }
  }

  replace(ret_aux, "ACTION_ID", action_id);
  replace(ret_aux, "CHECK_PREV_ACTIONS", check_actions);

  std::istringstream f(ret_aux);
  std::string line;
  while (std::getline(f, line)) {
    if (line != "") {
      ret = ret + t(l) + line + "\n";
    }
  }
  return ret;
}

void
STNBTBuilder::get_flow_dotgraph(
  GraphNode::Ptr node,
  std::set<std::string> & edges)
{
  for (const auto & arc : node->output_arcs) {
    auto child = std::get<0>(arc);
    std::string edge = std::to_string(node->node_num) + "->" + std::to_string(child->node_num) +
      ";\n";
    edges.insert(edge);
    get_flow_dotgraph(child, edges);
  }
}

std::string
STNBTBuilder::get_node_dotgraph(
  GraphNode::Ptr node,
  std::shared_ptr<std::map<std::string, ActionExecutionInfo>> action_map)
{
  std::stringstream ss;
  ss << t(2) << node->node_num << " [label=\"";
  ss << parser::pddl::nameActionsToString(node->action.action);
  ss << " " << to_string(node->action.type) << "\"";
  ss << "labeljust=c,style=filled";

  auto status = get_action_status(node->action, action_map);
  switch (status) {
    case ActionExecutor::RUNNING:
      ss << ",color=blue,fillcolor=skyblue";
      break;
    case ActionExecutor::SUCCESS:
      ss << ",color=green4,fillcolor=seagreen2";
      break;
    case ActionExecutor::FAILURE:
      ss << ",color=red,fillcolor=pink";
      break;
    case ActionExecutor::CANCELLED:
      ss << ",color=red,fillcolor=pink";
      break;
    case ActionExecutor::IDLE:
    case ActionExecutor::DEALING:
    default:
      ss << ",color=yellow3,fillcolor=lightgoldenrod1";
      break;
  }
  ss << "];\n";
  return ss.str();
}

ActionExecutor::Status
STNBTBuilder::get_action_status(
  ActionStamped action_stamped,
  std::shared_ptr<std::map<std::string, ActionExecutionInfo>> action_map)
{
  auto index = "(" + parser::pddl::nameActionsToString(action_stamped.action) + "):" +
    std::to_string(static_cast<int>(action_stamped.time * 1000));
  if (action_map->find(index) != action_map->end()) {
    if ((*action_map)[index].action_executor) {
      return (*action_map)[index].action_executor->get_internal_status();
    } else {
      return ActionExecutor::IDLE;
    }
  } else {
    return ActionExecutor::IDLE;
  }
}

std::string
STNBTBuilder::add_dot_graph_legend(
  int level_counter,
  int node_counter)
{
  std::stringstream ss;
  int legend_counter = level_counter;
  int legend_node_counter = node_counter;
  ss << t(1);
  ss << "subgraph cluster_" << legend_counter++ << " {\n";
  ss << t(2);
  ss << "label = \"Legend\";\n";

  ss << t(2);
  ss << "subgraph cluster_" << legend_counter++ << " {\n";
  ss << t(3);
  ss << "label = \"Plan Timestep (sec): X.X\";\n";
  ss << t(3);
  ss << "style = rounded;\n";
  ss << t(3);
  ss << "color = yellow3;\n";
  ss << t(3);
  ss << "bgcolor = lemonchiffon;\n";
  ss << t(3);
  ss << "labeljust = l;\n";
  ss << t(3);
  ss << legend_node_counter++ <<
    " [label=\n\"Finished action\n\",labeljust=c,style=filled,color=green4,fillcolor=seagreen2];\n";
  ss << t(3);
  ss << legend_node_counter++ <<
    " [label=\n\"Failed action\n\",labeljust=c,style=filled,color=red,fillcolor=pink];\n";
  ss << t(3);
  ss << legend_node_counter++ <<
    " [label=\n\"Current action\n\",labeljust=c,style=filled,color=blue,fillcolor=skyblue];\n";
  ss << t(3);
  ss << legend_node_counter++ << " [label=\n\"Future action\n\",labeljust=c,style=filled," <<
    "color=yellow3,fillcolor=lightgoldenrod1];\n";
  ss << t(2);
  ss << "}\n";

  ss << t(2);
  for (int i = node_counter; i < legend_node_counter; i++) {
    if (i > node_counter) {
      ss << "->";
    }
    ss << i;
  }
  ss << " [style=invis];\n";

  ss << t(1);
  ss << "}\n";

  return ss.str();
}

void
STNBTBuilder::print_graph(const plansys2::Graph::Ptr & graph) const
{
  print_node(graph->nodes.front(), 0);
}

void
STNBTBuilder::print_node(const plansys2::GraphNode::Ptr & node, int level) const
{
  std::cerr << t(level) << "[" << node->action.time << "]";
  std::cerr << " " << node->action.action->name;
  for (const auto & param : node->action.action->parameters) {
    std::cerr << " " << param.name;
  }
  std::cerr << " " << to_string(node->action.type) << std::endl;

  for (const auto & arc : node->output_arcs) {
    auto child = std::get<0>(arc);
    print_node(child, level + 1);
  }
}

void
STNBTBuilder::replace(
  std::string & str,
  const std::string & from,
  const std::string & to) const
{
  size_t start_pos = std::string::npos;
  while ((start_pos = str.find(from)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
  }
}

bool
STNBTBuilder::is_end(
  const std::tuple<GraphNode::Ptr, double, double> & edge,
  const ActionStamped & action) const
{
  const auto & node = std::get<0>(edge);
  auto t_1 = to_int_time(node->action.time, action_time_precision_ + 1);
  auto t_2 = to_int_time(action.time, action_time_precision_ + 1);
  return action.type == ActionType::START &&
         node->action.type == ActionType::END &&
         (t_1 == t_2) && (node->action.expression == action.expression);
}

std::string
STNBTBuilder::t(const int & level) const
{
  std::string ret;
  for (int i = 0; i < level; i++) {
    ret = ret + "  ";
  }
  return ret;
}

}  // namespace plansys2
