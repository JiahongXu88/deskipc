cmake_minimum_required(VERSION 3.15)

project(deskipc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if not specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(DESKIPC_BUILD_EXAMPLES "Build examples" ON)

add_subdirectory(core)

if (DESKIPC_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
