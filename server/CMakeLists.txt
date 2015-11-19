cmake_minimum_required(VERSION 2.8)

project(auth_server)
include_directories(../include)

set(EXECUTABLE_OUTPUT_PATH ../bin)
add_executable(login login.cc server.cc plugins.cc util.cc verify.cc)
target_link_libraries(login dl)
