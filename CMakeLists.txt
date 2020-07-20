
cmake_minimum_required(VERSION 3.1.6)

project(ph7 VERSION 2.1.4 LANGUAGES C)

add_library(ph7 STATIC "")

include_directories("${CMAKE_CURRENT_LIST_DIR}/inc")

target_sources(
    ph7
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/inc/ph7/ph7-amalg.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/ph7-amalg.c"
)

add_executable(ph7-test "${CMAKE_CURRENT_LIST_DIR}/tests/main.c")

target_link_libraries(ph7-test ph7)
