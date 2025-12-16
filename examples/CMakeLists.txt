add_executable(worker_cli worker_cli/main.cpp)
target_link_libraries(worker_cli PRIVATE deskipc_core)
target_compile_features(worker_cli PRIVATE cxx_std_17)

add_executable(parent_cli parent_cli/main.cpp)
target_link_libraries(parent_cli PRIVATE deskipc_core)
target_compile_features(parent_cli PRIVATE cxx_std_17)

