# platform check 
# default to x86 platform.  We'll check for X64 in a bit

if(NOT DEFINED __FIND_PLATFORM_LOADED)
    set(__FIND_PLATFORM_LOADED 1)
    SET(PLATFORM x86)
    SET(PLATFORM_SUFFIX "")

    # This definition is necessary to work around a bug with Intellisense described
    # here: http://tinyurl.com/2cb428.  Syntax highlighting is important for proper
    # debugger functionality.

    IF(CMAKE_SIZEOF_VOID_P MATCHES 8)
        #MESSAGE(STATUS "Detected 64-bit platform.")
        IF(WIN32)
            ADD_DEFINITIONS("-D_WIN64")
        ENDIF()
        SET(PLATFORM x86_64)
        SET(PLATFORM_SUFFIX "64")
    ELSE()
        #MESSAGE(STATUS "Detected 32-bit platform.")
    ENDIF()
endif ()


if (NOT PLATFORM_BUILD_PLATFORM_NAME)
    #set(PLATFORM_BUILD_PLATFORM_NAME "${CMAKE_C_COMPILER_ABI}")
    set(PLATFORM_BUILD_PLATFORM_NAME "${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
    string(TOLOWER "${PLATFORM_BUILD_PLATFORM_NAME}" PLATFORM_BUILD_PLATFORM_NAME)
    message(STATUS "Check: PLATFORM_BUILD_PLATFORM_NAME=${PLATFORM_BUILD_PLATFORM_NAME}")
else()
    message(STATUS "Custom: PLATFORM_BUILD_PLATFORM_NAME=${PLATFORM_BUILD_PLATFORM_NAME}")
endif()
