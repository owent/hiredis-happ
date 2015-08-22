
if(CYGWIN AND NOT LIBUV_ROOT)
    message(FATAL_ERROR "Libuv not support cygwin now")
endif()

# =========== 3rdparty libuv ==================
set (3RD_PARTY_LIBUV_BASE_DIR "${PROJECT_3RDPARTY_BUILD_DIR}")

FindConfigurePackage(
    PACKAGE Libuv
    BUILD_WITH_CONFIGURE
    MAKE_FLAGS "-j4"
    PREBUILD_COMMAND "./autogen.sh"
    WORKING_DIRECTORY "${PROJECT_3RDPARTY_BUILD_DIR}"
    PREFIX_DIRECTORY "${PROJECT_3RDPARTY_PREBUILT_DIR}"
    SRC_DIRECTORY_NAME "libuv-v1.7.1"
    TAR_URL "http://dist.libuv.org/dist/v1.7.1/libuv-v1.7.1.tar.gz"
)

if(Libuv_FOUND)
    include_directories(${Libuv_INCLUDE_DIRS})
    set (3RD_PARTY_LIBUV_LINK_NAME ${Libuv_LIBRARIES})
else()
    message(FATAL_ERROR "Libuv not found")
endif()

