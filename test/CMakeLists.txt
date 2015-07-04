include("${CMAKE_CURRENT_LIST_DIR}/test.macro.cmake")
# import gtest

enable_testing()
find_package(GTest)

# =========== find gtest ===========
if (GTEST_FOUND)
    message(STATUS "GTest Found: ${GTEST_LIBRARIES}")
	add_compiler_define(UTILS_TEST_MACRO_TEST_ENABLE_GTEST=1)
	list (APPEND EXTENTION_LINK_LIB ${GTEST_LIBRARIES})
    include_directories(${GTEST_INCLUDE_DIRS})

# =========== enable find boost.test ===========
elseif(ENABLE_BOOST_UNIT_TEST)
    
    find_package(Boost COMPONENTS unit_test_framework)
    set(Boost_AUTO_LIBS "${Boost_LIBRARIES}")
    
    if(Boost_FOUND)
        set(Boost_USE_STATIC_LIBS ON)
        find_package(Boost COMPONENTS unit_test_framework)
        if(NOT Boost_FOUND)
            set(Boost_USE_STATIC_LIBS OFF)
            find_package(Boost COMPONENTS unit_test_framework)
        elseif(NOT "${Boost_LIBRARIES}" EQUAL "${Boost_AUTO_LIBS}")
            set(Boost_USE_STATIC_LIBS OFF)
            find_package(Boost COMPONENTS unit_test_framework)
        endif()
        
        add_compiler_define(UTILS_TEST_MACRO_TEST_ENABLE_BOOST_TEST=1)
        list (APPEND EXTENTION_LINK_LIB ${Boost_LIBRARIES})
        include_directories(${Boost_INCLUDE_DIRS})
        message(STATUS "Boost.test Found: ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}")
        
        if (NOT Boost_USE_STATIC_LIBS)
            message(STATUS "Boost.test using dynamic library define BOOST_TEST_DYN_LINK")
            add_compiler_define(BOOST_TEST_DYN_LINK)
        endif()
    endif()
endif()

include_directories(${PROJECT_TEST_INC_DIR})

file(GLOB_RECURSE ALL_TEST_SRC_LIST 
    ${PROJECT_TEST_SRC_DIR}/*.h 
    ${PROJECT_TEST_SRC_DIR}/*.hpp 
    ${PROJECT_TEST_SRC_DIR}/*.c 
    ${PROJECT_TEST_SRC_DIR}/*.cpp
    ${PROJECT_TEST_SRC_DIR}/*.cc 
    ${PROJECT_TEST_SRC_DIR}/*.cxx
)
# source_group_by_dir(ALL_TEST_SRC_LIST)


# ================ multi thread ================

# ============ test - coroutine test frame ============
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/test")

add_executable(hiredis-happ-test ${ALL_TEST_SRC_LIST})
target_link_libraries(hiredis-happ-test
    hiredis-happ
    ${EXTENTION_LINK_LIB}
    ${PROJECT_3RDPARTY_LINK_NAME}
)

add_test(test hiredis-happ-test)

