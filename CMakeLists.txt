cmake_minimum_required(VERSION 2.8.12)
project(eigenpy)

find_package(catkin REQUIRED)

include(ExternalProject)

ExternalProject_Add(eigenpy
  GIT_REPOSITORY "https://github.com/stack-of-tasks/eigenpy.git"
  GIT_TAG "v1.5.1"

  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  TEST_COMMAND ""

  CMAKE_ARGS -DBUILD_UNIT_TESTS:BOOL=OFF -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_BINARY_DIR}/install #-DUSE_DEB_LAYOUT:BOOL=${SETUPTOOLS_DEB_LAYOUT}
)
ExternalProject_Add_Step(eigenpy CopyToDevel
  COMMENT "Copying to devel"
  COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/install/lib/libeigenpy.so ${CATKIN_DEVEL_PREFIX}/lib/
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CATKIN_DEVEL_PREFIX}/lib/python2.7/site-packages/
  COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/install/lib/python2.7/site-packages/eigenpy.so ${CATKIN_DEVEL_PREFIX}/lib/python2.7/site-packages/
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_BINARY_DIR}/install/include/${PROJECT_NAME}/ ${CATKIN_DEVEL_PREFIX}/include/${PROJECT_NAME}/
  DEPENDEES install
)

if(IS_DIRECTORY ${CMAKE_INSTALL_PREFIX})
  message(STATUS "Compiling for install workspace (${CMAKE_INSTALL_PREFIX})")
  catkin_package(
    INCLUDE_DIRS include/${PROJECT_NAME} ${EIGEN3_INCLUDE_DIR}
    LIBRARIES ${PYTHON_LIBRARIES} eigenpy
  )
else()
  message(STATUS "Compiling for devel workspace")
  file(MAKE_DIRECTORY ${CATKIN_DEVEL_PREFIX}/include/${PROJECT_NAME})
  catkin_package(
    INCLUDE_DIRS ${CATKIN_DEVEL_PREFIX}/include ${EIGEN3_INCLUDE_DIR}
    LIBRARIES ${PYTHON_LIBRARIES} eigenpy
  )
endif()

install(
  DIRECTORY ${CATKIN_DEVEL_PREFIX}/include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)
install(
  FILES ${CATKIN_DEVEL_PREFIX}/lib/libeigenpy.so
  DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
)
install(
  FILES ${CATKIN_DEVEL_PREFIX}/lib/python2.7/site-packages/eigenpy.so
  DESTINATION ${CATKIN_GLOBAL_PYTHON_DESTINATION}
)
