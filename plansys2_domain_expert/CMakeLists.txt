project(plansys2_domain_expert)

cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)

set(DEPS
  roscpp
  lifecycle
#  rclcpp_action
  plansys2_pddl_parser
  plansys2_msgs
  plansys2_core
  plansys2_popf_plan_solver
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

###########
## Build ##
###########

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

set(DOMAIN_EXPERT_SOURCES
  src/plansys2_domain_expert/DomainExpert.cpp
  src/plansys2_domain_expert/DomainExpertClient.cpp
  src/plansys2_domain_expert/DomainReader.cpp
  src/plansys2_domain_expert/DomainExpertNode.cpp
)

add_library(${PROJECT_NAME} SHARED ${DOMAIN_EXPERT_SOURCES})
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})

add_executable(domain_expert_node src/domain_expert_node.cpp)
target_link_libraries(domain_expert_node ${PROJECT_NAME})

#############
## Install ##
#############

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" 
)

install(DIRECTORY launch/
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/launch)

install(TARGETS
  ${PROJECT_NAME}
  domain_expert_node
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)


#if(BUILD_TESTING)
#  find_package(ament_lint_auto REQUIRED)
#  ament_lint_auto_find_test_dependencies()
#
#  find_package(ament_cmake_gtest REQUIRED)
#  add_subdirectory(test)
#endif()

#ament_export_include_directories(include)
#ament_export_libraries(${PROJECT_NAME})
#ament_export_dependencies(${dependencies})

#ament_package()
