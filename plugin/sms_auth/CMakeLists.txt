cmake_minimum_required(VERSION 2.8)

project(sms_auth)
add_library(sms_auth SHARED sms_auth.cc)
target_link_libraries(sms_auth sms)
