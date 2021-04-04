include_guard(GLOBAL)
# =========== 3rd_party redis ==================

macro(PROJECT_3RD_PARTY_REDIS_HIREDIS_IMPORT)
  if(TARGET hiredis::hiredis_ssl_static)
    message(STATUS "hiredis-happ using target: hiredis::hiredis_ssl_static")
    list(APPEND PROJECT_HIREDIS_HAPP_PUBLIC_LINK_NAMES hiredis::hiredis_ssl_static)
    if(TARGET hiredis::hiredis_static)
      project_build_tools_patch_imported_link_interface_libraries(hiredis::hiredis_ssl_static REMOVE_LIBRARIES hiredis::hiredis
                                                                  ADD_LIBRARIES hiredis::hiredis_static)
    endif()
  elseif(TARGET hiredis::hiredis_static)
    message(STATUS "hiredis-happ using target: hiredis::hiredis_static")
    list(APPEND PROJECT_HIREDIS_HAPP_PUBLIC_LINK_NAMES hiredis::hiredis_static)
  elseif(TARGET hiredis::hiredis_ssl)
    message(STATUS "hiredis-happ using target: hiredis::hiredis_ssl")
    list(APPEND PROJECT_HIREDIS_HAPP_PUBLIC_LINK_NAMES hiredis::hiredis_ssl)
    if(TARGET hiredis::hiredis)
      project_build_tools_patch_imported_link_interface_libraries(hiredis::hiredis_ssl REMOVE_LIBRARIES hiredis::hiredis_ssl_static
                                                                  ADD_LIBRARIES hiredis::hiredis)
    endif()
  elseif(TARGET hiredis::hiredis)
    message(STATUS "hiredis-happ using target: hiredis::hiredis")
    list(APPEND PROJECT_HIREDIS_HAPP_PUBLIC_LINK_NAMES hiredis::hiredis)
  else()
    message(STATUS "hiredis-happ can not find hiredis")
  endif()
endmacro()

# if (VCPKG_TOOLCHAIN) find_package(hiredis QUIET) find_package(hiredis_ssl QUIET NO_MODULE) PROJECT_3RD_PARTY_REDIS_HIREDIS_IMPORT() endif
# ()

if(NOT TARGET hiredis::hiredis_ssl_static
   AND NOT TARGET hiredis::hiredis_static
   AND NOT TARGET hiredis::hiredis_ssl
   AND NOT TARGET hiredis::hiredis)
  set(3RD_PARTY_REDIS_HIREDIS_VERSION "2a5a57b90a57af5142221aa71f38c08f4a737376") # v1.0.0 with some patch
  set(3RD_PARTY_REDIS_HIREDIS_DIR "${PROJECT_3RD_PARTY_PACKAGE_DIR}/hiredis-${3RD_PARTY_REDIS_HIREDIS_VERSION}")

  set(3RD_PARTY_REDIS_HIREDIS_BACKUP_FIND_ROOT ${CMAKE_FIND_ROOT_PATH})
  list(APPEND CMAKE_FIND_ROOT_PATH ${PROJECT_3RD_PARTY_INSTALL_DIR})

  set(3RD_PARTY_REDIS_HIREDIS_OPTIONS "-DDISABLE_TESTS=YES" "-DENABLE_EXAMPLES=OFF")
  if(OPENSSL_FOUND)
    list(APPEND 3RD_PARTY_REDIS_HIREDIS_OPTIONS "-DENABLE_SSL=ON")
  endif()

  findconfigurepackage(
    PACKAGE
    hiredis
    BUILD_WITH_CMAKE
    CMAKE_INHIRT_BUILD_ENV
    CMAKE_INHIRT_BUILD_ENV_DISABLE_CXX_FLAGS
    CMAKE_FLAGS
    ${3RD_PARTY_REDIS_HIREDIS_OPTIONS}
    "-DCMAKE_POSITION_INDEPENDENT_CODE=YES"
    WORKING_DIRECTORY
    "${PROJECT_3RD_PARTY_PACKAGE_DIR}"
    BUILD_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/deps/hiredis-${3RD_PARTY_REDIS_HIREDIS_VERSION}/build_jobs_${PROJECT_PREBUILT_PLATFORM_NAME}"
    PREFIX_DIRECTORY
    "${PROJECT_3RD_PARTY_INSTALL_DIR}"
    SRC_DIRECTORY_NAME
    "hiredis-${3RD_PARTY_REDIS_HIREDIS_VERSION}"
    GIT_BRANCH
    "${3RD_PARTY_REDIS_HIREDIS_VERSION}"
    GIT_URL
    "https://github.com/redis/hiredis.git")

  if(NOT hiredis_FOUND)
    echowithcolor(COLOR RED "-- Dependency: hiredis is required, we can not find prebuilt for hiredis and can not build from the sources")
    message(FATAL_ERROR "hiredis not found")
  else()
    if(TARGET hiredis::hiredis_static OR TARGET hiredis::hiredis)
      find_package(hiredis_ssl QUIET NO_MODULE)
    endif()
  endif()

  if(TARGET hiredis::hiredis_static OR TARGET hiredis::hiredis)
    find_package(hiredis_ssl QUIET NO_MODULE)
  endif()
  project_3rd_party_redis_hiredis_import()
else()
  project_3rd_party_redis_hiredis_import()
endif()

if(NOT hiredis_FOUND)
  echowithcolor(COLOR RED "-- Dependency: hiredis is required")
  message(FATAL_ERROR "hiredis not found")
endif()

if(3RD_PARTY_REDIS_HIREDIS_BACKUP_FIND_ROOT)
  set(CMAKE_FIND_ROOT_PATH ${3RD_PARTY_REDIS_HIREDIS_BACKUP_FIND_ROOT})
  unset(3RD_PARTY_REDIS_HIREDIS_BACKUP_FIND_ROOT)
endif()
