get_filename_component(SAMPLE_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

include_directories(${CMAKE_CURRENT_LIST_DIR})

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SAMPLE_SRC_FILES)

add_executable(${SAMPLE_NAME} ${SAMPLE_SRC_FILES})
target_link_libraries(${SAMPLE_NAME}
    hiredis-happ
    ${PROJECT_3RDPARTY_LINK_NAME}
)