add_library(deskipc_core STATIC
  protocol/framing.cpp
  rpc/rpc_client.cpp
  rpc/rpc_server.cpp
)

target_include_directories(deskipc_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}      # 允许 #include "protocol/..." "rpc/..." "transport/..."
    ${CMAKE_SOURCE_DIR}/third_party  # 允许 #include <nlohmann/json.hpp>
)

target_compile_features(deskipc_core PUBLIC cxx_std_17)

if (WIN32)
  target_link_libraries(deskipc_core PUBLIC Ws2_32)
endif()

