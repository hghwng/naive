cmake_minimum_required(VERSION 2.8)

project(auth_server)
include_directories(../include)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ../bin)
add_library(pam_naive SHARED server.cc plugins.cc util.cc verify.cc)
target_link_libraries(pam_naive dl)
target_link_libraries(pam_naive pam)