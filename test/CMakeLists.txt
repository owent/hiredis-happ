echowithcolor(COLOR GREEN "-- Configure Unit Test ${CMAKE_CURRENT_LIST_DIR}")

include("${PROJECT_TEST_BAS_DIR}/test.build_bin.cmake")
find_package(Libevent)

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

atframe_add_test_executable(hiredis-happ-test ${PROJECT_TEST_SRC_LIST})

unset(PROJECT_TEST_EXT_LIBS)
if(ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUV_LINK_NAME)
  add_compiler_define(HIREDIS_HAPP_ENABLE_LIBUV=1)
  list(APPEND PROJECT_TEST_EXT_LIBS ${ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUV_LINK_NAME})
  if(WIN32)
    list(APPEND PROJECT_TEST_EXT_LIBS psapi iphlpapi userenv ws2_32)
  endif()
elseif(Libevent_FOUND OR ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUNWIND_LINK_NAME)
  add_compiler_define(HIREDIS_HAPP_ENABLE_LIBEVENT=1)
  if(ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUNWIND_LINK_NAME)
    list(APPEND PROJECT_TEST_EXT_LIBS ${ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_LIBUNWIND_LINK_NAME})
  else()
    list(APPEND PROJECT_TEST_EXT_LIBS ${Libevent_LIBRARIES})
    include_directories(${Libevent_INCLUDE_DIRS})
  endif()
endif()

if(PROJECT_TEST_EXT_LIBS AND NOT MSVC)
  list(APPEND PROJECT_TEST_EXT_LIBS pthread)
endif()

set_target_properties(
  hiredis-happ-test
  PROPERTIES INSTALL_RPATH_USE_LINK_PATH YES
             BUILD_WITH_INSTALL_RPATH NO
             BUILD_RPATH_USE_ORIGIN YES)

target_link_libraries(hiredis-happ-test hiredis-happ ${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME} ${PROJECT_TEST_EXT_LIBS}
                      ${COMPILER_OPTION_EXTERN_CXX_LIBS})

add_test(NAME hiredis-happ-run-test COMMAND hiredis-happ-test -f happ_cmd* -f happ_connection* -f happ_cluster* -f
                                            happ_raw*)
set_tests_properties(hiredis-happ-run-test PROPERTIES LABELS "unit")

add_test(NAME hiredis-happ-redis-integration-raw COMMAND hiredis-happ-test -f happ_integration_raw*)
set_tests_properties(hiredis-happ-redis-integration-raw PROPERTIES LABELS "integration;redis" TIMEOUT 120)

add_test(NAME hiredis-happ-redis-integration-cluster COMMAND hiredis-happ-test -f happ_integration_cluster*)
set_tests_properties(hiredis-happ-redis-integration-cluster PROPERTIES LABELS "integration;redis;cluster" TIMEOUT 180)
