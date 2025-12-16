add_executable(deskipc_tests
  deskipc_tests.cpp
)

target_link_libraries(deskipc_tests PRIVATE deskipc_core)

add_test(NAME deskipc_tests COMMAND deskipc_tests)
