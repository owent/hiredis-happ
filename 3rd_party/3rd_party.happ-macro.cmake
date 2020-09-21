# 特殊的依赖项
# =========== installer ===========
if (NOT PROJECT_3RD_PARTY_PACKAGE_DIR)
    set (PROJECT_3RD_PARTY_PACKAGE_DIR "${CMAKE_CURRENT_LIST_DIR}/packages")
endif ()
if (NOT PROJECT_3RD_PARTY_INSTALL_DIR)
    set (PROJECT_3RD_PARTY_INSTALL_DIR "${CMAKE_CURRENT_LIST_DIR}/install/${PROJECT_PREBUILT_PLATFORM_NAME}")
endif ()

if (NOT EXISTS ${PROJECT_3RD_PARTY_PACKAGE_DIR})
    file(MAKE_DIRECTORY ${PROJECT_3RD_PARTY_PACKAGE_DIR})
endif ()

if (NOT EXISTS ${PROJECT_3RD_PARTY_INSTALL_DIR})
    file(MAKE_DIRECTORY ${PROJECT_3RD_PARTY_INSTALL_DIR})
endif ()

# 弱依赖 libuv
# 弱依赖 libevent

# 强依赖 libhiredis
include("${CMAKE_CURRENT_LIST_DIR}/3rd_party.libhiredis.cmake")