# 自动构建hiredis

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.10")
    include_guard(GLOBAL)
endif()

if(NOT 3RD_PARTY_REDIS_HIREDIS_VERSION)
    set (3RD_PARTY_REDIS_HIREDIS_VERSION "v0.14.1")
endif()

set (3RD_PARTY_REDIS_BASE_DIR ${PROJECT_3RDPARTY_BUILD_DIR})

if (NOT TARGET hiredis)
    if (NOT EXISTS "${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${3RD_PARTY_REDIS_HIREDIS_VERSION}/.git")
        message(STATUS "hiredis not found try to pull it.")
        find_package(Git)
        if(GIT_FOUND)
            message(STATUS "git found: ${GIT_EXECUTABLE}")
            execute_process(COMMAND ${GIT_EXECUTABLE} clone --depth=100 -b ${3RD_PARTY_REDIS_HIREDIS_VERSION} "https://github.com/redis/hiredis" hiredis-${3RD_PARTY_REDIS_HIREDIS_VERSION}
                WORKING_DIRECTORY ${3RD_PARTY_REDIS_BASE_DIR}
            )
        endif()
    endif()

    set(DISABLE_TESTS ON)
    set(ENABLE_EXAMPLES OFF)
    add_subdirectory("${3RD_PARTY_REDIS_BASE_DIR}/hiredis-${3RD_PARTY_REDIS_HIREDIS_VERSION}")
    unset(DISABLE_TESTS)
    unset(ENABLE_EXAMPLES)
endif ()
