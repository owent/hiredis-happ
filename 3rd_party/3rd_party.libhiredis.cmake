# 自动构建hiredis

set (3RD_PARTY_REDIS_BASE_DIR ${PROJECT_3RDPARTY_BUILD_DIR})

find_package(Libhiredis)

if(NOT LIBHIREDIS_FOUND)
    if (LIBHIREDIS_INCLUDE_DIRS AND LIBHIREDIS_LIBRARIES)
        set(Libhiredis_INCLUDE_DIRS ${LIBHIREDIS_INCLUDE_DIRS})
        set(Libhiredis_LIBRARIES ${LIBHIREDIS_LIBRARIES})
        get_filename_component(LIBHIREDIS_ROOT ${Libhiredis_INCLUDE_DIRS} DIRECTORY)
        message(STATUS "Use hiredis(inc=${Libhiredis_INCLUDE_DIRS}, lib=${Libhiredis_LIBRARIES})")
        set(LIBHIREDIS_FOUND ON)
    elseif(Libhiredis_INCLUDE_DIRS AND Libhiredis_LIBRARIES)
        get_filename_component(LIBHIREDIS_ROOT ${Libhiredis_INCLUDE_DIRS} DIRECTORY)
        message(STATUS "Use hiredis(inc=${Libhiredis_INCLUDE_DIRS}, lib=${Libhiredis_LIBRARIES})")
        set(LIBHIREDIS_FOUND ON)
    else()
        set(LIBHIREDIS_ROOT ${PROJECT_3RDPARTY_PREBUILT_DIR})
        message(STATUS ${PROJECT_3RDPARTY_PREBUILT_DIR})
        find_package(Libhiredis)
    endif()
endif()

if(LIBHIREDIS_USING_SRC)
    add_compiler_define(LIBHIREDIS_USING_SRC=1)
endif()

if(LIBHIREDIS_FOUND)
    list(APPEND PROJECT_3RDPARTY_LINK_NAME ${Libhiredis_LIBRARIES})
    include_directories(${Libhiredis_INCLUDE_DIRS})
else()
    message(STATUS "hiredis not found try to build it.")
    if (NOT EXISTS "${3RD_PARTY_REDIS_BASE_DIR}/hiredis")
        find_package(Git)
        if(GIT_FOUND)
            message(STATUS "git found: ${GIT_EXECUTABLE}")
            execute_process(COMMAND ${GIT_EXECUTABLE} clone --depth=1 "https://github.com/owent-contrib/hiredis" hiredis
                WORKING_DIRECTORY ${3RD_PARTY_REDIS_BASE_DIR}
            )
        endif()
    else()
        message(STATUS "use cache package ${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${HIREDIS_VERSION}.tar.gz")
    endif()

    execute_process(COMMAND ${CMAKE_MAKE_PROGRAM} "PREFIX=${PROJECT_3RDPARTY_PREBUILT_DIR}" install
        WORKING_DIRECTORY "${3RD_PARTY_REDIS_BASE_DIR}/hiredis"
    )

    # win32 cmake bug
    if(WIN32 AND EXISTS "${PROJECT_3RDPARTY_PREBUILT_DIR}/lib/libhiredis.a")
        file(COPY "${PROJECT_3RDPARTY_PREBUILT_DIR}/lib/libhiredis.a" DESTINATION ${PROJECT_3RDPARTY_BUILD_DIR})
        file(RENAME "${PROJECT_3RDPARTY_BUILD_DIR}/libhiredis.a" "${PROJECT_3RDPARTY_PREBUILT_DIR}/lib/libhiredis.lib")
    endif()

    set(LIBHIREDIS_ROOT ${PROJECT_3RDPARTY_PREBUILT_DIR})
    find_package(Libhiredis)
    if(LIBHIREDIS_FOUND)
        list(APPEND PROJECT_3RDPARTY_LINK_NAME ${Libhiredis_LIBRARIES})
        include_directories(${Libhiredis_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "lib hiredis is required")
    endif()
endif()
