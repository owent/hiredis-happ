echowithcolor(COLOR GREEN "-- Configure Unit Test ${CMAKE_CURRENT_LIST_DIR}")

include("${PROJECT_TEST_BAS_DIR}/test.build_bin.cmake")

file(
  GLOB_RECURSE
  PROJECT_TEST_SRC_LIST
  ${PROJECT_TEST_SRC_DIR}/app/*.cpp
  ${PROJECT_TEST_SRC_DIR}/frame/*.h
  ${PROJECT_TEST_SRC_DIR}/frame/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/*.hpp
  ${CMAKE_CURRENT_LIST_DIR}/*.h
  ${CMAKE_CURRENT_LIST_DIR}/*.c
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/*.cc
  ${CMAKE_CURRENT_LIST_DIR}/*.cxx)
source_group_by_dir(PROJECT_TEST_SRC_LIST)

# ============ test - coroutine test frame ============
if(NOT (WIN32 AND BUILD_SHARED_LIBS))
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/test")
endif()

add_compiler_define(_CRT_SECURE_NO_WARNINGS=1)
add_compiler_define(HIREDIS_HAPP_UNIT_TEST_HACK=1)

add_executable(hiredis-happ-test ${PROJECT_TEST_SRC_LIST})

set_target_properties(
  hiredis-happ-test
  PROPERTIES INSTALL_RPATH_USE_LINK_PATH YES
             BUILD_WITH_INSTALL_RPATH NO
             BUILD_RPATH_USE_ORIGIN YES)

target_link_libraries(hiredis-happ-test hiredis-happ ${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME}
                      ${COMPILER_OPTION_EXTERN_CXX_LIBS})

add_test(NAME hiredis-happ-run-test COMMAND hiredis-happ-test)
