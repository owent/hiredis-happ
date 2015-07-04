
# =========== 环境检查 ===========  
# x86 or x86_64
if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set (ARCH_SUFFIX "64")
else()
	set (ARCH_SUFFIX "")
endif()

# 设置实际的默认编译输出目录 
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib${ARCH_SUFFIX}")

link_directories("${PROJECT_SOURCE_DIR}/lib${ARCH_SUFFIX}")
