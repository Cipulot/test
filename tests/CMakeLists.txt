cmake_minimum_required(VERSION 3.16)
project(tabvia_tests C)
enable_testing()
add_executable(protocol_test protocol_test.c
  ../components/via_protocol/via_protocol.c
  ../components/definitions/definition_source.c
  ../components/hid_console/hid_console.c
  ../components/hid_usage/hid_usage.c
  ../components/via_transport/via_transport.c)
target_include_directories(protocol_test PRIVATE
  ../components/via_protocol/include ../components/definitions/include
  ../components/hid_console/include ../components/hid_usage/include
  ../components/via_transport/include)
target_compile_options(protocol_test PRIVATE -Wall -Wextra -Werror)
add_test(NAME protocol_test COMMAND protocol_test)
