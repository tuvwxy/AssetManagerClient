cmake_minimum_required(VERSION 2.8)
set(PROJECT_NAME amclient)
project(${PROJECT_NAME})

if( CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE )
message(FATAL_ERROR "In-source builds are not allowed.
Please create a directory and run cmake from there, passing the path
to this source directory as the last argument. For example,
$ mkdir build; cd build
$ cmake ..
This process created the file `CMakeCache.txt' and the directory `CMakeFiles'.
Please delete them.")
endif()

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeModules)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system thread date_time regex REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIR})
  set(LINK_LIBRARIES ${LINK_LIBRARIES} ${Boost_LIBRARIES})
endif()

include_directories(include)
include_directories(third_party/include)
add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(tests)

