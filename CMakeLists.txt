cmake_minimum_required (VERSION 3.12)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

add_compile_options("/FC")

project("jcs")

add_library(buffer "buffer.cpp" "buffer.hpp")

add_library(serial "serial.cpp" "serial.hpp")

add_library(index "index.cpp" "index.hpp")
target_link_libraries(index buffer serial)

# Add source to this project's executable.
add_executable(jcs "jcs.cpp")
target_link_libraries(jcs index)
