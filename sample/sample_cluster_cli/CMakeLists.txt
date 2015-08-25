find_package(Libevent)
find_package(Libuv)

if(Libuv_FOUND OR Libevent_FOUND)

    get_filename_component(SAMPLE_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

    include_directories(${CMAKE_CURRENT_LIST_DIR})

    aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SAMPLE_SRC_FILES)

    if(Libuv_FOUND)
        add_compiler_define(HIREDIS_HAPP_ENABLE_LIBUV=1)
        set(SAMPLE_EXT_LIBS ${Libuv_LIBRARIES})
    elseif(Libevent_FOUND)
        add_compiler_define(HIREDIS_HAPP_ENABLE_LIBEVENT=1)
        set(SAMPLE_EXT_LIBS ${Libevent_LIBRARIES})
    endif()

    add_executable(${SAMPLE_NAME} ${SAMPLE_SRC_FILES})
    target_link_libraries(${SAMPLE_NAME}
        hiredis-happ
        ${SAMPLE_EXT_LIBS}
        ${PROJECT_3RDPARTY_LINK_NAME}
    )

endif()