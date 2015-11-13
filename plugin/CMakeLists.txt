cmake_minimum_required(VERSION 2.8)

project (plugins)
include_directories(../include)
set(LIBRARY_OUTPUT_PATH ../../bin/plugins)

add_subdirectory(example_custauth)
add_subdirectory(example_sysauth)
