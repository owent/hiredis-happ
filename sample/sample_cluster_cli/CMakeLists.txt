if(NOT WIN32 AND (NOT CYGWIN OR LIBUV_ROOT))

    get_filename_component(SAMPLE_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

    include("${PROJECT_3RDPARTY_BAS_DIR}/3rd_party.libuv.cmake")

    include_directories(${CMAKE_CURRENT_LIST_DIR})

    aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SAMPLE_SRC_FILES)

    add_executable(${SAMPLE_NAME} ${SAMPLE_SRC_FILES})
    target_link_libraries(${SAMPLE_NAME}
        hiredis-happ
        ${3RD_PARTY_LIBUV_LINK_NAME}
        ${PROJECT_3RDPARTY_LINK_NAME}
    )

endif()