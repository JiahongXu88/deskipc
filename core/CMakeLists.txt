add_library(deskipc_core STATIC
  protocol/framing.cpp
)

target_include_directories(deskipc_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (WIN32)
  target_link_libraries(deskipc_core PUBLIC Ws2_32)
endif()
