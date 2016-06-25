cmake_minimum_required(VERSION 2.8.9)

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

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
link_directories (${LIBRARY_OUTPUT_PATH})

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
			
