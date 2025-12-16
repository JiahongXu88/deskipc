cmake_minimum_required(VERSION 3.15)

project(deskipc LANGUAGES CXX)

# ---- Testing ----
option(DESKIPC_BUILD_TESTS "Build tests" ON)

if (DESKIPC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if not specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if (MSVC)
  add_compile_options(/utf-8)
endif()


option(DESKIPC_BUILD_EXAMPLES "Build examples" ON)

add_subdirectory(core)

if (DESKIPC_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
