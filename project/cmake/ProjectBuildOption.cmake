# 默认配置选项
#####################################################################
option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." OFF)
option(ENABLE_BOOST_UNIT_TEST "Enable boost unit test." OFF)

# ============ fetch cmake toolset ============
include("${CMAKE_CURRENT_LIST_DIR}/FetchDependeny.cmake")
include(IncludeDirectoryRecurse)
include(EchoWithColor)
