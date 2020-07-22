
cmake_minimum_required(VERSION 3.1.6)

project(ph7 VERSION 2.1.4 LANGUAGES C)

add_library(ph7 STATIC "")

include_directories("${CMAKE_CURRENT_LIST_DIR}/inc")

target_sources(
    ph7
    PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/inc/ph7/ph7.h"
    "${CMAKE_CURRENT_LIST_DIR}/inc/ph7/ph7.h"
    "${CMAKE_CURRENT_LIST_DIR}/inc/ph7/ph7int.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/vm.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/vfs.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/parse.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/oo.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/memobj.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/lib.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/lex.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/hashmap.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/constant.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/compile.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/builtin.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ph7/api.c"
)

add_executable(ph7-test "${CMAKE_CURRENT_LIST_DIR}/tests/main.c")

target_link_libraries(ph7-test ph7)
