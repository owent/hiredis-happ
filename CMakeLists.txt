cmake_minimum_required(VERSION 3.7.0)
cmake_policy(SET CMP0054 NEW)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
    cmake_policy(SET CMP0074 NEW)
endif()
enable_testing()

project(hiredis-happ)


# 准备下载依赖组件
include ("${CMAKE_CURRENT_LIST_DIR}/3rd_party/atframe_utils/libatframe_utils.prepare.cmake")

########################################################################
# CMake 模块 (递归包含模块, 带颜色输出模块, 平台检测模块)
set (PROJECT_CMAKE_MODULE_DIR "${3RD_PARTY_ATFRAME_UTILS_PKG_DIR}/project/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_CMAKE_MODULE_DIR}/modules")


include("${PROJECT_CMAKE_MODULE_DIR}/modules/IncludeDirectoryRecurse.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/modules/EchoWithColor.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/modules/FindConfigurePackage.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/project/cmake/ProjectBuildOption.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/FindPlatform.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/ProjectTools.cmake")


#####################################################################
# 导入编译器和编译选项配置
include("${PROJECT_CMAKE_MODULE_DIR}/CompilerOption.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/TargetOption.cmake")
EchoWithColor(COLOR GREEN "-- Build Type: ${CMAKE_BUILD_TYPE}")


########################################################################
# 导入项目配置
## 导入所有 macro 定义
include ("${CMAKE_CURRENT_LIST_DIR}/3rd_party/3rd_party.happ-macro.cmake")

# ================ multi thread ================
find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT)
    add_definitions(-D_POSIX_MT_)
    if(NOT ANDROID)
        list(APPEND COMPILER_OPTION_EXTERN_CXX_LIBS pthread)
    endif ()
    if (THREADS_PREFER_PTHREAD_FLAG)
        add_definitions(${THREADS_PREFER_PTHREAD_FLAG})
    endif ()
endif ()

# 设置输出目录 
set(PROJECT_ALL_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(PROJECT_ALL_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src")
include_directories (${PROJECT_ALL_INCLUDE_DIR})

file(GLOB_RECURSE SRC_LIST "${PROJECT_ALL_SOURCE_DIR}/*.c" "${PROJECT_ALL_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE HEADER_LIST "${PROJECT_ALL_INCLUDE_DIR}/*.h" "${PROJECT_ALL_INCLUDE_DIR}/*.hpp")

add_library(hiredis-happ ${SRC_LIST})

install(TARGETS hiredis-happ
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
	
install(DIRECTORY ${PROJECT_ALL_INCLUDE_DIR} DESTINATION .
	FILES_MATCHING REGEX ".+\\.h(pp)?$"
)

if (PROJECT_ENABLE_SAMPLE)
    include ("${CMAKE_CURRENT_LIST_DIR}/sample/sample.happ-macro.cmake")
    add_subdirectory("${PROJECT_SOURCE_DIR}/sample")
endif()

if (PROJECT_ENABLE_UNITTEST)
    include ("${CMAKE_CURRENT_LIST_DIR}/test/test.happ-macro.cmake")
    add_subdirectory("${PROJECT_SOURCE_DIR}/test")
endif()
			
