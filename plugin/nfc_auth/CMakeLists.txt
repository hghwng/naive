cmake_minimum_required(VERSION 2.8)

project (nfc_auth)
add_library(nfc_auth SHARED nfc_auth.cc)
target_link_libraries(nfc_auth nfc)
