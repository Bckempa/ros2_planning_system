project(plansys2_msgs)

cmake_minimum_required(VERSION 3.5)

set(MSG_DEPS
  std_msgs
  actionlib_msgs
  )

find_package(catkin2 REQUIRED COMPONENTS
  message_generation
  ${MSG_DEPS}
)

add_message_files(
  DIRECTORY msg
  FILES
  Action.msg
  ActionExecution.msg
  ActionExecutionInfo.msg
  ActionPerformerStatus.msg
  DurativeAction.msg
  Knowledge.msg
  Node.msg
  Param.msg
  Plan.msg
  PlanItem.msg
  Tree.msg
)

add_service_files(
  FILES
  AddProblem.srv
  AddProblemGoal.srv
  AffectNode.srv
  AffectParam.srv
  ExistNode.srv
  GetDomain.srv
  GetDomainActions.srv
  GetDomainActionDetails.srv
  GetDomainDurativeActionDetails.srv
  GetDomainName.srv
  GetDomainTypes.srv
  GetDomainConstants.srv
  GetNodeDetails.srv
  GetPlan.srv
  GetOrderedSubGoals.srv
  GetProblem.srv
  GetProblemGoal.srv
  GetProblemInstances.srv
  GetProblemInstanceDetails.srv
  GetStates.srv
  IsProblemGoalSatisfied.srv
  RemoveProblemGoal.srv
  ClearProblemKnowledge.srv
)

add_action_files (
  DIRECTORY action
  FILES
  ExecutePlan.action
)

generate_messages(
  DEPENDENCIES
  ${MSG_DEPS}
)

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES
  CATKIN_DEPENDS message_runtime ${MSG_DEPS}
  DEPENDS
)

#############
## Install ##
#############

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)