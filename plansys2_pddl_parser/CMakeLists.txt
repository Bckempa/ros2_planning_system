project(plansys2_pddl_parser)

cmake_minimum_required(VERSION 3.5)

set(DEPS
  plansys2_msgs
  std_msgs
  roscpp
  )

set(CMAKE_CXX_STANDARD 17)

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

set(PDDL-PARSER-SOURCES
  src/plansys2_pddl_parser/GroundFunc.cpp
  src/plansys2_pddl_parser/When.cpp
  src/plansys2_pddl_parser/Function.cpp
  src/plansys2_pddl_parser/Expression.cpp
  src/plansys2_pddl_parser/ParamCond.cpp
  src/plansys2_pddl_parser/Action.cpp
  src/plansys2_pddl_parser/TemporalAction.cpp
  src/plansys2_pddl_parser/Or.cpp
  src/plansys2_pddl_parser/Exists.cpp
  src/plansys2_pddl_parser/Not.cpp
  src/plansys2_pddl_parser/Derived.cpp
  src/plansys2_pddl_parser/And.cpp
  src/plansys2_pddl_parser/Lifted.cpp
  src/plansys2_pddl_parser/Forall.cpp
  src/plansys2_pddl_parser/Oneof.cpp
  src/plansys2_pddl_parser/FunctionModifier.cpp
  src/plansys2_pddl_parser/Ground.cpp
  src/plansys2_pddl_parser/Utils.cpp
  src/plansys2_pddl_parser/TypeGround.cpp
)

add_library(${PROJECT_NAME} SHARED ${PDDL-PARSER-SOURCES})
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})

add_executable(parser src/parser.cpp)
target_link_libraries(parser ${PROJECT_NAME})


install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(TARGETS
  ${PROJECT_NAME}
  parser
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

#if(BUILD_TESTING)
#  find_package(ament_cmake_gtest REQUIRED)
#  find_package(ament_index_cpp REQUIRED)
#  set(dependencies ${dependencies} ament_index_cpp)
#  add_subdirectory(test)
#endif()

