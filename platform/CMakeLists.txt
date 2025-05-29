if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PLATFORM_DIR "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(PLATFORM_DIR "linux")
else()
  message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

add_library(memory_mapped_file
    "memory_mapped_file.hpp"
    "${PLATFORM_DIR}/memory_mapped_file.cpp"
)
target_include_directories(memory_mapped_file PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
