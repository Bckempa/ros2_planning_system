project(plansys2_bt_actions)

set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.5)


find_package(ZMQ)
if(ZMQ_FOUND)
    message(STATUS "ZeroMQ found.")
    add_definitions(-DZMQ_FOUND)
else()
  message(WARNING "ZeroMQ NOT found. Not including PublisherZMQ.")
endif()

set(CMAKE_CXX_STANDARD 17)

set(dependencies
    roscpp
 #   rclcpp_action
    lifecycle
    plansys2_executor
    behaviortree_cpp
#    action_msgs
    lifecycle_msgs
)

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

set(BT_ACTIONS_SOURCES
  src/plansys2_bt_actions/BTAction.cpp
)

add_library(${PROJECT_NAME} SHARED ${BT_ACTIONS_SOURCES})
target_link_libraries(${PROJECT_NAME} ${ZMQ_LIBRARIES} ${catkin_LIBRARIES})

add_executable(bt_action_node  src/bt_action_node.cpp)
target_link_libraries(bt_action_node ${PROJECT_NAME} ${catkin_LIBRARIES})

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(TARGETS
  bt_action_node
  ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

#if(BUILD_TESTING)
#  find_package(ament_lint_auto REQUIRED)
#  ament_lint_auto_find_test_dependencies()

#  find_package(ament_cmake_gtest REQUIRED)
#  find_package(plansys2_msgs REQUIRED)
#  find_package(test_msgs REQUIRED)
#  find_package(geometry_msgs REQUIRED)

#  set(dependencies ${dependencies} plansys2_msgs test_msgs geometry_msgs)

#  add_subdirectory(test)
#endif()

