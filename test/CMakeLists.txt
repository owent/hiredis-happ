
EchoWithColor(COLOR GREEN "-- Configure Unit Test ${CMAKE_CURRENT_LIST_DIR}")

## 导入工程项目
if(TARGET ${3RD_PARTY_ATFRAME_UTILS_LINK_NAME})
    message(STATUS "[hiredis-happ] ${3RD_PARTY_ATFRAME_UTILS_LINK_NAME} already exist, use it directly.")
else()
    if (NOT EXISTS "${CMAKE_BINARY_DIR}/deps/${3RD_PARTY_ATFRAME_UTILS_LINK_NAME}")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/deps/${3RD_PARTY_ATFRAME_UTILS_LINK_NAME}")
    endif ()
    add_subdirectory(${3RD_PARTY_ATFRAME_UTILS_PKG_DIR} "${CMAKE_BINARY_DIR}/deps/${3RD_PARTY_ATFRAME_UTILS_LINK_NAME}")
endif()

include("${PROJECT_TEST_BAS_DIR}/test.build_bin.cmake")

include_directories(${3RD_PARTY_ATFRAME_UTILS_INC_DIR} ${PROJECT_LIBATBUS_ROOT_SRC_DIR})

file(GLOB_RECURSE PROJECT_TEST_SRC_LIST
    ${PROJECT_TEST_SRC_DIR}/app/*.cpp
    ${PROJECT_TEST_SRC_DIR}/frame/*.h
    ${PROJECT_TEST_SRC_DIR}/frame/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/*.hpp
    ${CMAKE_CURRENT_LIST_DIR}/*.h
    ${CMAKE_CURRENT_LIST_DIR}/*.c
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/*.cxx
)
source_group_by_dir(PROJECT_TEST_SRC_LIST)

# ============ test - coroutine test frame ============
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/test")

add_compiler_define(_CRT_SECURE_NO_WARNINGS=1)
add_compiler_define(HIREDIS_HAPP_UNIT_TEST_HACK=1)

add_executable(hiredis-happ-test ${PROJECT_TEST_SRC_LIST})
target_link_libraries(hiredis-happ-test
    hiredis-happ
    ${3RD_PARTY_ATFRAME_UTILS_LINK_NAME}
    ${PROJECT_3RDPARTY_LINK_NAME}
    ${COMPILER_OPTION_EXTERN_CXX_LIBS}
)

add_test(test hiredis-happ-test)
