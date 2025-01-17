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

#ifndef PLANSYS2_PDDL_PARSER__UTILS_H_
#define PLANSYS2_PDDL_PARSER__UTILS_H_

#include <plansys2_msgs/shared_ptr_typedefs.h>

#include <plansys2_msgs/Action.h>
#include <plansys2_msgs/DurativeAction.h>
#include <plansys2_msgs/Node.h>
#include <plansys2_msgs/Param.h>
#include <plansys2_msgs/Tree.h>

namespace parser
{
namespace pddl
{

/// Removes newlines, duplicated spaces, tabs and spaces from parenthesis
/**
 * \param[in] expr The expression to be reduced
 * \return The reduced expression
 */
std::string getReducedString(const std::string & expr);

/// Returns node type corresponding to the string input
/**
 * \param[in] input The input string
 * \return The node type
 */
uint8_t getNodeType(const std::string & expr, uint8_t def = plansys2_msgs::Node::UNKNOWN);

/// Returns expression type and start position of an expression in a string
/**
 * \param[in] input The input string
 * \return result <- tuple(uint8_t, int)
 *         result(0) The expression type
 *         result(1) The start position of the first occurrence
 */
std::tuple < uint8_t, int > getExpr(const std::string & input);

/// Returns expression type corresponding to the string input
/**
 * \param[in] input The input string
 * \return The expression type
 */
uint8_t getExprType(const std::string & input);

/// Returns function modifier type and start position of a function modifier in a string
/**
 * \param[in] input The input string
 * \return result <- tuple(uint8_t, int)
 *         result(0) The function modifier type
 *         result(1) The start position of the first occurrence
 */
std::tuple < uint8_t, int > getFunMod(const std::string & input);

/// Returns function modifier type corresponding to the string input
/**
 * \param[in] input The input string
 * \return The function modifier type
 */
uint8_t getFunModType(const std::string & input);

int getParenthesis(const std::string & wexpr, int start);

std::vector<std::string> getSubExpr(const std::string & expr);

std::string nameActionsToString(const std::shared_ptr<plansys2_msgs::Action> action);

std::string nameActionsToString(const std::shared_ptr<plansys2_msgs::DurativeAction> action);

std::string toString(const plansys2_msgs::Tree & tree, uint32_t node_id = 0, bool negate = false);

std::string toString(const plansys2_msgs::Node & node);

std::string toStringPredicate(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

std::string toStringFunction(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

std::string toStringNumber(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

std::string toStringAnd(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

std::string toStringOr(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

std::string toStringNot(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

std::string toStringExpression(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

std::string toStringFunctionModifier(const plansys2_msgs::Tree & tree, uint32_t node_id, bool negate);

/// This function creates a complete tree.
/**
 * This function recursivelly extracts the logic expressions and predicates from the expression.
 *
 * \param[in] tree The tree object to be created
 * \param[in] expr A string containing predicates and logic operators
 * \param[in] construct A string containing the associated PDDL constructs
 * \return A smart pointer to the node created
*/
plansys2_msgs::NodeSharedPtr fromString(plansys2_msgs::Tree & tree, const std::string & expr, bool negate = false, uint8_t parent = plansys2_msgs::Node::UNKNOWN);

plansys2_msgs::Tree fromString(const std::string & expr, bool negate = false, uint8_t parent = plansys2_msgs::Node::UNKNOWN);

plansys2_msgs::Node fromStringPredicate(const std::string & predicate);

plansys2_msgs::Node fromStringFunction(const std::string & function);

plansys2_msgs::Param fromStringParam(const std::string & name, const std::string & type = {});

plansys2_msgs::Tree fromPredicates(const std::vector<std::string> & preds);

plansys2_msgs::TreeSharedPtr fromSubtree(const plansys2_msgs::Tree & subtree, uint8_t node_type);

plansys2_msgs::TreeSharedPtr fromSubtrees(const std::vector<plansys2_msgs::Tree> & subtrees, uint8_t node_type);

std::vector<uint32_t> getSubtreeIds(const plansys2_msgs::Tree & tree);

std::vector<plansys2_msgs::Tree> getSubtrees(const plansys2_msgs::Tree & tree);

void getSubtreeChildren(plansys2_msgs::Tree & subtree, const plansys2_msgs::Tree & tree, uint32_t tree_parent, uint32_t subtree_parent);

void getPredicates(std::vector<plansys2_msgs::Node> & predicates, const plansys2_msgs::Tree & tree, uint32_t node_id = 0, bool negate = false);

void getFunctions(std::vector<plansys2_msgs::Node> & functions, const plansys2_msgs::Tree & tree, uint32_t node_id = 0, bool negate = false);

bool checkTreeEquality(const plansys2_msgs::Tree & first, const plansys2_msgs::Tree & second);

bool checkNodeEquality(const plansys2_msgs::Node & first, const plansys2_msgs::Node & second);

bool checkParamEquality(const plansys2_msgs::Param & first, const plansys2_msgs::Param & second);

bool empty(const plansys2_msgs::Tree & tree);

}  // namespace pddl
}  // namespace parser

#endif  // PLANSYS2_PDDL_PARSER__UTILS_H_
