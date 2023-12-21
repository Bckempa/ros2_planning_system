project(plansys2_lifecycle_manager)

set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.5)


set(dependencies
    roscpp
    lifecycle
    lifecycle_msgs
)

find_package(catkin2 REQUIRED COMPONENTS
  ${dependencies}
)  

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES ${PROJECT_NAME}
  CATKIN_DEPENDS ${dependencies}
  DEPENDS
)

###########
## Build ##
###########

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)


add_library(${PROJECT_NAME}
  src/plansys2_lifecycle_manager/lifecycle_manager.cpp
)
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})


add_executable(lifecycle_manager_node  src/lifecycle_manager_node.cpp)
target_link_libraries(lifecycle_manager_node ${PROJECT_NAME} ${catkin_LIBRARIES})

#############
## Install ##
#############

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(TARGETS
  ${PROJECT_NAME}
  lifecycle_manager_node
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
