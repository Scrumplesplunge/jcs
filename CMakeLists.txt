cmake_minimum_required (VERSION 3.12)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

add_compile_options("/FC")

project ("simple-tags")

find_package(asio CONFIG REQUIRED)

# Add source to this project's executable.
add_executable (simple-tags "simple-tags.cpp")
set_property(TARGET simple-tags PROPERTY CXX_STANDARD 20)
target_link_libraries(simple-tags asio::asio)
