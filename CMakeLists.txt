cmake_minimum_required (VERSION 2.6)
project (forwarder)

set (FORWARDER_VERSION_MAJOR 1)
set (FORWARDER_VERSION_MINOR 0)
set (CMAKE_CXX_STANDARD 11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

FILE(GLOB_RECURSE MY_HEADERS "${PROJECT_SOURCE_DIR}/include/*.h")
source_group("My Headers" FILES ${MY_HEADERS})

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/enet")
add_subdirectory (enet)
set (EXTRA_LIBS ${EXTRA_LIBS} enet)

SET(SRC_LIST main.cpp src/utils.cpp src/uniqid.cpp src/forwardctrl.cpp)

add_executable(forwarder ${SRC_LIST})
target_link_libraries (forwarder ${EXTRA_LIBS})
if (WIN32)
  target_link_libraries (forwarder winmm.lib ws2_32.lib)
endif (WIN32)

install (TARGETS forwarder DESTINATION bin)