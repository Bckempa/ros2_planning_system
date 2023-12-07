project(plansys2_bringup)

cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)

set(dependencies
    roscpp
    plansys2_domain_expert
    plansys2_problem_expert
    plansys2_planner
    plansys2_executor
    plansys2_lifecycle_manager
    behaviortree_cpp
)

find_package(catkin REQUIRED COMPONENTS
  ${dependencies}
)  

catkin_package(
  INCLUDE_DIRS #include
  LIBRARIES 
  CATKIN_DEPENDS ${dependencies}
  DEPENDS
)

include_directories(
#  include
  ${catkin_INCLUDE_DIRS}
)
  

#add_executable(plansys2_node
#  src/plansys2_node.cpp
#)
#target_link_libraries(plansys2_node ${catkin_LIBRARIES})
#target_compile_definitions(plansys2_node PUBLIC "PLUGINLIB__DISABLE_BOOST_FUNCTIONS")

#if(BUILD_TESTING)
#  find_package(ament_lint_auto REQUIRED)
#  ament_lint_auto_find_test_dependencies()
#endif()

foreach( dir params launch)
   install( DIRECTORY ${dir}/
         DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}/${dir} )
endforeach(dir)


install(TARGETS
#  plansys2_node
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
  )

