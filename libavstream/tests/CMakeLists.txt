cmake_minimum_required(VERSION 3.8)
project(libavstream_tests)

add_subdirectory(common/testapi)
add_subdirectory(common/imgui)
add_subdirectory(common/renderer)

add_subdirectory(test_server)
add_subdirectory(test_client)
