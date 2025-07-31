cmake_minimum_required (VERSION 3.12)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project("jcs")

add_subdirectory("platform")

add_library(serial "serial.cpp" "serial.hpp")

add_library(index "index.cpp" "index.hpp")
target_link_libraries(index memory_mapped_file serial)

# Add source to this project's executable.
add_executable(jcs "jcs.cpp")
target_link_libraries(jcs index)

install(TARGETS jcs)
