find_package(Libevent)
find_package(Libuv)

if(Libuv_FOUND OR Libevent_FOUND)

    get_filename_component(SAMPLE_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
    set(SAMPLE_NAME "hiredis-happ-${SAMPLE_NAME}")

    include_directories(${CMAKE_CURRENT_LIST_DIR})

    aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SAMPLE_SRC_FILES)

    if(Libuv_FOUND)
        add_compiler_define(HIREDIS_HAPP_ENABLE_LIBUV=1)
        set(SAMPLE_EXT_LIBS ${Libuv_LIBRARIES})
        include_directories(${Libuv_INCLUDE_DIRS})
    elseif(Libevent_FOUND)
        add_compiler_define(HIREDIS_HAPP_ENABLE_LIBEVENT=1)
        set(SAMPLE_EXT_LIBS ${Libevent_LIBRARIES})
        include_directories(${Libevent_INCLUDE_DIRS})
    endif()

    if (NOT MSVC)
        set(SAMPLE_EXT_LIBS ${SAMPLE_EXT_LIBS} pthread)
    endif()
    
    add_executable(${SAMPLE_NAME} ${SAMPLE_SRC_FILES})
    target_link_libraries(${SAMPLE_NAME}
        hiredis-happ
        ${SAMPLE_EXT_LIBS}
        ${COMPILER_OPTION_EXTERN_CXX_LIBS}
    )

endif()