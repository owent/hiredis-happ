# 自动构建hiredis


set (3RD_PARTY_REDIS_BASE_DIR ${PROJECT_3RDPARTY_BUILD_DIR})

find_package(Libhiredis)

if(NOT LIBHIREDIS_FOUND)
    set(LIBHIREDIS_ROOT ${PROJECT_3RDPARTY_PREBUILT_DIR})
    find_package(Libhiredis)
endif()

if(LIBHIREDIS_FOUND)
    list(APPEND PROJECT_3RDPARTY_LINK_NAME ${Libhiredis_LIBRARIES})
    include_directories(${Libhiredis_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "fatal")
    message(STATUS "hiredis not found try to build it.")
    if (NOT EXISTS "${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${HIREDIS_VERSION}.tar.gz")
        message(STATUS "download from https://github.com/redis/hiredis/archive/v${HIREDIS_VERSION}.tar.gz to ${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${HIREDIS_VERSION}.tar.gz")
        file(DOWNLOAD "https://github.com/redis/hiredis/archive/v${HIREDIS_VERSION}.tar.gz" "${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${HIREDIS_VERSION}.tar.gz" SHOW_PROGRESS)
    else()
        message(STATUS "use cache package ${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${HIREDIS_VERSION}.tar.gz")
    endif()

    find_program(TAR_EXECUTABLE tar PATHS "${CYGWIN_INSTALL_PATH}/bin")
    execute_process(COMMAND ${TAR_EXECUTABLE} -axvf hiredis-${HIREDIS_VERSION}.tar.gz
        WORKING_DIRECTORY ${3RD_PARTY_REDIS_BASE_DIR}
    )

    execute_process(COMMAND make "PREFIX=${PROJECT_3RDPARTY_PREBUILT_DIR}" install
        WORKING_DIRECTORY "${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${HIREDIS_VERSION}"
    )

    set(LIBHIREDIS_ROOT ${PROJECT_3RDPARTY_PREBUILT_DIR})
    find_package(Libhiredis)
    if(LIBHIREDIS_FOUND)
        list(APPEND PROJECT_3RDPARTY_LINK_NAME ${Libhiredis_LIBRARIES})
        include_directories(${Libhiredis_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "lib hiredis is required")
    endif()
endif()
