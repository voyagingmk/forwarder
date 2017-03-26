cmake_minimum_required (VERSION 3.5)


SET(buildtarget "dll" CACHE STRING "buildtarget = exe/dll/staic/")

if(UNIX)
    MESSAGE(STATUS "It is UNIX-like OS")  
    SET (CMAKE_C_COMPILER             "/usr/bin/clang")
    SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++")
endif()

project (forwarder)

set (FORWARDER_VERSION_MAJOR 1)
set (FORWARDER_VERSION_MINOR 0)
set (CMAKE_CXX_STANDARD 11)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

FILE(GLOB_RECURSE MY_HEADERS "${PROJECT_SOURCE_DIR}/include/*.h")
source_group("My Headers" FILES ${MY_HEADERS})

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/include/zlib")
include_directories("${PROJECT_SOURCE_DIR}/csharp-binding")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/zlib")
include_directories("${PROJECT_SOURCE_DIR}/enet")
add_subdirectory (enet)
set (EXTRA_LIBS ${EXTRA_LIBS} enet)


FILE(GLOB_RECURSE BASE_SOURCERS 
  ${PROJECT_SOURCE_DIR}/src/*.cpp 
  ${PROJECT_SOURCE_DIR}/src/*.c 
  ${PROJECT_SOURCE_DIR}/zlib/*.c
  )

if(buildtarget STREQUAL "dll")
  FILE(GLOB_RECURSE MAIN_SOURCERS ${PROJECT_SOURCE_DIR}/csharp-binding/*.cpp)
else()
  FILE(GLOB_RECURSE MAIN_SOURCERS ${PROJECT_SOURCE_DIR}/main/*.cpp)
endif()

if(buildtarget STREQUAL "dll")
  add_library(forwarder SHARED ${BASE_SOURCERS} ${MAIN_SOURCERS})
else()
  add_executable(forwarder ${BASE_SOURCERS} ${MAIN_SOURCERS})
endif()


target_link_libraries (forwarder ${EXTRA_LIBS})
if (WIN32)
  target_link_libraries (forwarder winmm.lib ws2_32.lib)
endif (WIN32)

install (TARGETS forwarder DESTINATION bin)
