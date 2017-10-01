
# ============ sample - [...] ============
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/sample")

file(GLOB ALL_SAMPLE_DIRS "${PROJECT_SAMPLE_BAS_DIR}/*")
foreach(SAMPLE_DIR ${ALL_SAMPLE_DIRS})
    get_filename_component(SAMPLE_NAME ${SAMPLE_DIR} NAME)
    string(SUBSTRING ${SAMPLE_NAME} 0 7 SAMPLE_NAME_PREFIX)
    if (${SAMPLE_NAME_PREFIX} STREQUAL "sample_")
        message(STATUS "Sample Found: ${SAMPLE_NAME}")
        add_subdirectory(${SAMPLE_DIR})
    endif()
endforeach()