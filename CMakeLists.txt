cmake_minimum_required (VERSION 2.6)
project (forwarder)

# The version number.
set (FORWARDER_VERSION_MAJOR 1)
set (FORWARDER_VERSION_MINOR 0)

include_directories("${PROJECT_SOURCE_DIR}/include")

include_directories("${PROJECT_SOURCE_DIR}/enet")
add_subdirectory (enet)
set (EXTRA_LIBS ${EXTRA_LIBS} libenet)

# add the executable
add_executable(forwarder main.cpp)
target_link_libraries (forwarder  ${EXTRA_LIBS})