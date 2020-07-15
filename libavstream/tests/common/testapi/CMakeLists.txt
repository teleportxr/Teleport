cmake_minimum_required(VERSION 3.8)

add_library(testapi INTERFACE)
target_compile_features(testapi INTERFACE cxx_std_17)
target_include_directories(testapi INTERFACE include)
