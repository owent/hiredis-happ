include_guard(GLOBAL)

if(NOT ATFRAME_UTILS_ROOT)
  include(FetchContent)

  if(PROJECT_GIT_REMOTE_ORIGIN_USE_SSH AND NOT PROJECT_GIT_CLONE_REMOTE_ORIGIN_DISABLE_SSH)
    set(ATFRAME_UTILS_GIT_REPOSITORY "git@github.com:atframework/atframe_utils.git")
  else()
    set(ATFRAME_UTILS_GIT_REPOSITORY "https://github.com/atframework/atframe_utils.git")
  endif()

  FetchContent_Populate(
    "download_atframe_utils"
    SOURCE_DIR "${PROJECT_3RD_PARTY_PACKAGE_DIR}/atframe_utils-default/repo"
    BINARY_DIR "${CMAKE_BINARY_DIR}/deps/atframe_utils/build_jobs_${PROJECT_PREBUILT_PLATFORM_NAME}"
    SUBBUILD_DIR "${CMAKE_BINARY_DIR}/deps/download_atframe_utils"
    GIT_REPOSITORY "${ATFRAME_UTILS_GIT_REPOSITORY}"
    GIT_TAG "origin/master"
    GIT_REMOTE_NAME "origin"
    GIT_SHALLOW TRUE)

  set(ATFRAME_UTILS_ROOT "${download_atframe_utils_SOURCE_DIR}")
endif()

set(3RD_PARTY_ATFRAME_UTILS_LINK_NAME atframe_utils)
