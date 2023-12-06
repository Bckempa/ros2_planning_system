project(plansys2_terminal)
cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)

set(DEPS
    roscpp
    lifecycle
    plansys2_msgs
    plansys2_domain_expert
    plansys2_problem_expert
    plansys2_planner
    plansys2_executor
    plansys2_pddl_parser
)

find_package(catkin REQUIRED COMPONENTS
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

add_library(terminal SHARED
  src/plansys2_terminal/Terminal.cpp)
target_compile_definitions(terminal PUBLIC "PLUGINLIB__DISABLE_BOOST_FUNCTIONS")

add_executable(${PROJECT_NAME}
  src/terminal_node.cpp)
target_link_libraries(${PROJECT_NAME} terminal readline ${catkin_LIBRARIES})

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(TARGETS
  terminal
  ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

#if(BUILD_TESTING)
#  find_package(ament_lint_auto REQUIRED)
#  ament_lint_auto_find_test_dependencies()
#
#  find_package(ament_cmake_gtest REQUIRED)
#  find_package(ament_index_cpp REQUIRED)
#  find_package(lifecycle_msgs REQUIRED)
#
#  set(dependencies ${dependencies} ament_index_cpp lifecycle_msgs)
#
#  add_subdirectory(test)
#endif()

