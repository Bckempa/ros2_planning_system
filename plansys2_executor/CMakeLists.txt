project(plansys2_executor)

set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.5)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(dependencies
    roscpp
    lifecycle
    lifecycle_msgs
#    rclcpp_cascade_lifecycle
    actionlib
    plansys2_pddl_parser
    plansys2_core
    plansys2_msgs
    plansys2_domain_expert
    plansys2_problem_expert
    plansys2_planner
    pluginlib
    behaviortree_cpp
    std_msgs
)



find_package(ZMQ)
if(ZMQ_FOUND)
    message(STATUS "ZeroMQ found.")
    add_definitions(-DZMQ_FOUND)
else()
  message(WARNING "ZeroMQ NOT found. Not including PublisherZMQ.")
endif()

find_package(catkin REQUIRED COMPONENTS
  ${dependencies}
)  

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES ${PROJECT_NAME}
  CATKIN_DEPENDS ${dependencies}
  DEPENDS
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${ZMQ_INCLUDE_DIRS}
)
 

set(EXECUTOR_SOURCES
  src/plansys2_executor/ExecutorClient.cpp
  src/plansys2_executor/ActionExecutor.cpp
  src/plansys2_executor/ActionExecutorClient.cpp
  src/plansys2_executor/ExecutorNode.cpp
  src/plansys2_executor/behavior_tree/execute_action_node.cpp
  src/plansys2_executor/behavior_tree/wait_action_node.cpp
  src/plansys2_executor/behavior_tree/check_action_node.cpp
  src/plansys2_executor/behavior_tree/wait_atstart_req_node.cpp
  src/plansys2_executor/behavior_tree/check_overall_req_node.cpp
  src/plansys2_executor/behavior_tree/check_atend_req_node.cpp
  src/plansys2_executor/behavior_tree/check_timeout_node.cpp
  src/plansys2_executor/behavior_tree/apply_atstart_effect_node.cpp
  src/plansys2_executor/behavior_tree/apply_atend_effect_node.cpp
)


add_library(${PROJECT_NAME} SHARED ${EXECUTOR_SOURCES})
target_link_libraries(${PROJECT_NAME} ${ZMQ_LIBRARIES})

add_executable(executor_node src/executor_node.cpp)
target_link_libraries(executor_node ${PROJECT_NAME} ${catkin_LIBRARIES})

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

add_library(bt_builder_plugins SHARED
  src/plansys2_executor/bt_builder_plugins/simple_bt_builder.cpp
  src/plansys2_executor/bt_builder_plugins/stn_bt_builder.cpp
  )
target_link_libraries(bt_builder_plugins ${catkin_LIBRARIES})
target_compile_definitions(bt_builder_plugins PRIVATE BT_PLUGIN_EXPORT)
##ament_target_dependencies(bt_builder_plugins ${dependencies})

#pluginlib_export_plugin_description_file(plansys2_executor plugins.xml)


foreach( dir launch behavior_trees)
   install( DIRECTORY ${dir}/
         DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/${dir} )
endforeach(dir)

install(TARGETS
  ${PROJECT_NAME}
  bt_builder_plugins
  executor_node
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)



#if(BUILD_TESTING)
#  find_package(ament_lint_auto REQUIRED)
#  ament_lint_auto_find_test_dependencies()

#  find_package(ament_cmake_gtest REQUIRED)

#  add_subdirectory(test)
#endif()

