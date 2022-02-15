cmake_minimum_required(VERSION 3.16.0)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0022 NEW)
cmake_policy(SET CMP0067 NEW)
cmake_policy(SET CMP0074 NEW)

enable_testing()

project(
  hiredis-happ
  VERSION "0.9.0"
  HOMEPAGE_URL "https://github.com/owent/hiredis-happ"
  LANGUAGES C CXX)

include("${CMAKE_CURRENT_LIST_DIR}/project/cmake/ProjectBuildOption.cmake")
include(MaybePopulateSubmodule)
include("${CMAKE_CURRENT_LIST_DIR}/third_party/Repository.cmake")
if(PROJECT_HIREDIS_HAPP_ENABLE_UNITTEST OR BUILD_TESTING)
  include("${CMAKE_CURRENT_LIST_DIR}/atframework/Repository.cmake")
endif()
echowithcolor(COLOR GREEN "-- Build Type: ${CMAKE_BUILD_TYPE}")

# ######################################################################################################################
# 导入项目配置 导入所有 macro 定义
set(PROJECT_HIREDIS_HAPP_PUBLIC_LINK_NAMES ${ATFRAMEWORK_CMAKE_TOOLSET_THIRD_PARTY_HIREDIS_LINK_NAME})
unset(PROJECT_HIREDIS_HAPP_INTERFACE_LINK_NAMES)
unset(PROJECT_HIREDIS_HAPP_PRIVATE_COMPILE_OPTIONS)

# ================ multi thread ================
find_package(Threads)
if(CMAKE_USE_PTHREADS_INIT)
  if(NOT ANDROID)
    if(TARGET Threads::Threads)
      list(APPEND PROJECT_HIREDIS_HAPP_PUBLIC_LINK_NAMES Threads::Threads)
    endif()
  endif()
endif()

# 设置输出目录
set(PROJECT_HIREDIS_HAPP_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(PROJECT_HIREDIS_HAPP_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src")
set(HIREDIS_HAPP_VERSION "${PROJECT_VERSION}")

configure_file("${PROJECT_HIREDIS_HAPP_INCLUDE_DIR}/detail/hiredis_happ_config.h.in"
               "${PROJECT_HIREDIS_HAPP_INCLUDE_DIR}/detail/hiredis_happ_config.h" @ONLY)

file(GLOB_RECURSE PROJECT_HIREDIS_HAPP_SRC_LIST "${PROJECT_HIREDIS_HAPP_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE PROJECT_HIREDIS_HAPP_HEADER_LIST "${PROJECT_HIREDIS_HAPP_INCLUDE_DIR}/*.h")

include(GNUInstallDirs)

if(BUILD_SHARED_LIBS OR ATFRAMEWORK_USE_DYNAMIC_LIBRARY)
  add_library(hiredis-happ SHARED ${PROJECT_HIREDIS_HAPP_SRC_LIST} ${PROJECT_HIREDIS_HAPP_HEADER_LIST})
  set_target_properties(
    hiredis-happ
    PROPERTIES C_VISIBILITY_PRESET "hidden"
               CXX_VISIBILITY_PRESET "hidden"
               VERSION ${HIREDIS_HAPP_VERSION}
               SOVERSION ${HIREDIS_HAPP_VERSION}
               INTERFACE_COMPILE_DEFINITIONS HIREDIS_HAPP_API_DLL=1)
  target_compile_definitions(hiredis-happ PRIVATE HIREDIS_HAPP_API_NATIVE=1 HIREDIS_HAPP_API_DLL=1)
  target_compile_options(hiredis-happ PRIVATE ${COMPILER_STRICT_EXTRA_CFLAGS} ${COMPILER_STRICT_CFLAGS})
else()
  add_library(hiredis-happ STATIC ${PROJECT_HIREDIS_HAPP_SRC_LIST} ${PROJECT_HIREDIS_HAPP_HEADER_LIST})
  set_target_properties(
    hiredis-happ
    PROPERTIES C_VISIBILITY_PRESET "hidden"
               CXX_VISIBILITY_PRESET "hidden"
               VERSION ${HIREDIS_HAPP_VERSION})
  target_compile_definitions(hiredis-happ PRIVATE HIREDIS_HAPP_API_NATIVE=1)
  target_compile_options(hiredis-happ PRIVATE ${COMPILER_STRICT_EXTRA_CFLAGS} ${COMPILER_STRICT_CFLAGS})
endif()

set_target_properties(
  hiredis-happ
  PROPERTIES INSTALL_RPATH_USE_LINK_PATH YES
             BUILD_WITH_INSTALL_RPATH NO
             BUILD_RPATH_USE_ORIGIN YES)

target_include_directories(hiredis-happ PUBLIC "$<BUILD_INTERFACE:${PROJECT_HIREDIS_HAPP_INCLUDE_DIR}>"
                                               "$<INSTALL_INTERFACE:include>")

if(PROJECT_THIRD_PARTY_INSTALL_DIR)
  target_include_directories(hiredis-happ PRIVATE "$<BUILD_INTERFACE:${PROJECT_THIRD_PARTY_INSTALL_DIR}/include>")
endif()

if(WIN32)
  list(APPEND PROJECT_HIREDIS_HAPP_INTERFACE_LINK_NAMES psapi iphlpapi userenv ws2_32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND PROJECT_HIREDIS_HAPP_INTERFACE_LINK_NAMES dl rt)
elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|NetBSD|OpenBSD")
  list(APPEND PROJECT_HIREDIS_HAPP_INTERFACE_LINK_NAMES kvm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  list(APPEND PROJECT_HIREDIS_HAPP_INTERFACE_LINK_NAMES kstat nsl sendfile socket)
endif()

target_link_libraries(
  hiredis-happ
  PUBLIC ${PROJECT_HIREDIS_HAPP_PUBLIC_LINK_NAMES}
  INTERFACE ${PROJECT_HIREDIS_HAPP_INTERFACE_LINK_NAMES})

if(PROJECT_HIREDIS_HAPP_PRIVATE_COMPILE_OPTIONS)
  target_compile_options(hiredis-happ PRIVATE ${PROJECT_HIREDIS_HAPP_PRIVATE_COMPILE_OPTIONS})
endif()

if(MSVC)
  add_target_properties(hiredis-happ COMPILE_OPTIONS /ZI)
endif()

install(
  TARGETS hiredis-happ
  EXPORT hiredis-happ-target
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
  DIRECTORY ${PROJECT_HIREDIS_HAPP_INCLUDE_DIR}
  DESTINATION .
  FILES_MATCHING
  REGEX ".+\\.h(pp)?$"
  PATTERN ".svn" EXCLUDE
  PATTERN ".git" EXCLUDE)

export(
  TARGETS hiredis-happ
  FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/cmake/hiredis-happ-target.cmake"
  NAMESPACE hiredis::)

install(
  EXPORT hiredis-happ-target
  NAMESPACE "hiredis::"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake")

if(PROJECT_HIREDIS_HAPP_ENABLE_SAMPLE)
  include("${CMAKE_CURRENT_LIST_DIR}/sample/sample.happ-macro.cmake")
  add_subdirectory("${PROJECT_SOURCE_DIR}/sample")
endif()

if(PROJECT_HIREDIS_HAPP_ENABLE_UNITTEST OR BUILD_TESTING)
  include("${CMAKE_CURRENT_LIST_DIR}/test/test.happ-macro.cmake")
  add_subdirectory("${PROJECT_SOURCE_DIR}/test")
endif()
