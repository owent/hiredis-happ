# 自动构建hiredis

set (3RD_PARTY_REDIS_BASE_DIR ${PROJECT_3RDPARTY_BUILD_DIR})

find_package(Libhiredis)

if(NOT LIBHIREDIS_FOUND)
    set(LIBHIREDIS_ROOT ${PROJECT_3RDPARTY_PREBUILT_DIR})
    message(STATUS ${PROJECT_3RDPARTY_PREBUILT_DIR})
    find_package(Libhiredis)
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
            execute_process(COMMAND ${GIT_EXECUTABLE} clone "https://github.com/owent-contrib/hiredis" hiredis
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
