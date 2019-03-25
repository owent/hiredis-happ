cmake_minimum_required(VERSION 3.7.0)
cmake_policy(SET CMP0054 NEW)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
    cmake_policy(SET CMP0074 NEW)
endif()
enable_testing()

project(hiredis-happ)
set (PROJECT_CMAKE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}/project/cmake")
list (APPEND CMAKE_MODULE_PATH "${PROJECT_CMAKE_MODULE_DIR}/modules")

include("${PROJECT_CMAKE_MODULE_DIR}/modules/FindConfigurePackage.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/modules/IncludeDirectoryRecurse.cmake")

# 编译选项 
#####################################################################
include("${PROJECT_CMAKE_MODULE_DIR}/ProjectBuildOption.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/CompilerOption.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/TargetOption.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/FindPlatform.cmake")

# 包含所有macro文件
include_macro_recurse(FILTER "*.happ-macro.cmake" ${PROJECT_SOURCE_DIR})

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
    add_subdirectory("${PROJECT_SOURCE_DIR}/sample")
endif()

if (PROJECT_ENABLE_UNITTEST)
    add_subdirectory("${PROJECT_SOURCE_DIR}/test")
endif()
			
