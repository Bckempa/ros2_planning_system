project(plansys2_planner)

set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.5)

set(DEPS
  roscpp
#  rclcpp_action
  lifecycle
  plansys2_core
  plansys2_pddl_parser
  plansys2_msgs
  plansys2_domain_expert
  plansys2_problem_expert
  plansys2_popf_plan_solver
  pluginlib
  std_msgs
  lifecycle_msgs
)


find_package(catkin2 REQUIRED COMPONENTS
  ${DEPS}
)  

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES ${PROJECT_NAME}
  CATKIN_DEPENDS ${DEPS}
  DEPENDS
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

set(PLANNER_SOURCES
  src/plansys2_planner/PlannerClient.cpp
  src/plansys2_planner/PlannerNode.cpp
)

add_library(${PROJECT_NAME} SHARED ${PLANNER_SOURCES})
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})

add_executable(planner_node src/planner_node.cpp)
target_compile_definitions(planner_node PUBLIC "PLUGINLIB__DISABLE_BOOST_FUNCTIONS")
target_link_libraries(planner_node ${PROJECT_NAME})

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

foreach( dir launch)
   install( DIRECTORY ${dir}/
         DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/${dir} )
endforeach(dir)

install(TARGETS
  ${PROJECT_NAME}
  planner_node
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

#ament_export_include_directories(include)
#ament_export_libraries(${PROJECT_NAME})
#ament_export_dependencies(${dependencies})

#ament_package()
