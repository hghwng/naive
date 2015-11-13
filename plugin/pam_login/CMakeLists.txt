cmake_minimum_required(VERSION 2.8)

project (pam_login)
add_library(pam_login SHARED pam_login.cc)
target_link_libraries(pam_login pam)