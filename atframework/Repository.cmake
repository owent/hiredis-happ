include_guard(GLOBAL)

if(TARGET atframe_utils)
  set(ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME atframe_utils)
elseif(TARGET atframework::atframe_utils)
  set(ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME atframework::atframe_utils)
else()
  set(ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME atframe_utils)
  if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/_deps/${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/_deps/${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME}")
  endif()
  maybe_populate_submodule(ATFRAMEWORK_ATFRAME_UTILS "atframework/atframe_utils"
                           "${PROJECT_SOURCE_DIR}/atframework/atframe_utils")
  add_subdirectory("${ATFRAMEWORK_ATFRAME_UTILS_REPO_DIR}"
                   "${CMAKE_CURRENT_BINARY_DIR}/_deps/${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME}")
endif()
