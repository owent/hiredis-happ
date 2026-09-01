# ============ sample - [...] ============
if(NOT PROJECT_SAMPLE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/sample")
endif()

file(GLOB ALL_SAMPLE_DIRS "${PROJECT_SAMPLE_BAS_DIR}/*")
foreach(SAMPLE_DIR ${ALL_SAMPLE_DIRS})
  get_filename_component(SAMPLE_NAME ${SAMPLE_DIR} NAME)
  string(SUBSTRING ${SAMPLE_NAME} 0 7 SAMPLE_NAME_PREFIX)
  if(${SAMPLE_NAME_PREFIX} STREQUAL "sample_")
    message(STATUS "Sample Found: ${SAMPLE_NAME}")
    add_subdirectory(${SAMPLE_DIR})
    if(PROJECT_SAMPLE_RUNTIME_OUTPUT_DIRECTORY AND TARGET "hiredis-happ-${SAMPLE_NAME}")
      if(COMMAND atframe_target_set_runtime_output_directory)
        atframe_target_set_runtime_output_directory("hiredis-happ-${SAMPLE_NAME}"
                                                    "${PROJECT_SAMPLE_RUNTIME_OUTPUT_DIRECTORY}")
      else()
        set_target_properties("hiredis-happ-${SAMPLE_NAME}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                                       "${PROJECT_SAMPLE_RUNTIME_OUTPUT_DIRECTORY}")
      endif()
    endif()
  endif()
endforeach()
