add_executable(worker_cli worker_cli/main.cpp)
target_link_libraries(worker_cli PRIVATE deskipc_core)

add_executable(parent_cli parent_cli/main.cpp)
target_link_libraries(parent_cli PRIVATE deskipc_core)
